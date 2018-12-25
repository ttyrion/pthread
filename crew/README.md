
### Record
#### pthread_cleanup_push、pthread_cleanup_pop
From Linux Man Page: 
1. POSIX.1 permits pthread_cleanup_push() and pthread_cleanup_pop() to be implemented as macros that expand to text containing '{' and '}', respectively.  For this reason, the caller must ensure that calls to these functions are paired within the same function, and at the same lexical nesting level.

2. Calling longjmp(3) (siglongjmp(3)) produces undefined results if any call has been made to pthread_cleanup_push() or pthread_cleanup_pop() without the matching call of the pair.

上面第一点，**编译期**：成对使用pthread_cleanup_push() 和 pthread_cleanup_pop() 才能保证代码通过编译（如果它们被实现为宏）。
上面第二点，**运行期**：成对调用pthread_cleanup_push() 和 pthread_cleanup_pop() 才能保证代码正确性。pthread_cleanup_push宏很可能做了一些工作，例如在栈上分配了内存。这些内存会被释放，或者被下一个函数调用重写，如果这发生在 pthread_cleanup_pop 之前，很可能就导致崩溃。crew示例代码里面就由此导致必现的崩溃。

#### Object Member Function as Thread Routine or Clean Routine
crew示例代码中把一个线程封装为一个Worker对象，线程函数即为Worker的operator()，线程清理函数为Worker类的Clean成员函数。实现方式如下：

```C
/* thread routine template */
template <class T>
void *WorkerRoutine(void *pt) {
    return (*(reinterpret_cast<T*>(pt)))();
};

template <class T>
void WorkerCleanRoutine(void *pt) {
    return ((reinterpret_cast<T*>(pt)->Clean()));
};

/* Object Member Function as Thread Routine */
int status = pthread_create(&thread_, NULL, WorkerRoutine<Worker>, this);
if (status != 0) {
    LOG_RETURN(false, id_ <<  " create pthread failed @" << status << ".");
}

/* Object Member Function as Clean Routine */
pthread_cleanup_push(WorkerCleanRoutine<Worker>, this);
...
pthread_cleanup_pop(0);

```

#### condition variable
**Condition variable** does not provide a way of **mutual exclusion**, it provides **notification**. What provides mutual exclusion is the **mutex**, which synchronizes the access to shared data. 

Here's some rools:
1. A mutex can be used with **more than one** condition variables. And that's why they are implemented and created independently. In the sample code of crew: the mutex mutex_ of a Crew object would be bound to both the condition variables done_ and go_, and also mutex_ would be used independently too for synchronization.

2. A condition variable should be bound with **one and only one** predicate. 

3. The predicate should be checked **before** and **after** the waiter thread returning from pthread_cond_wait/pthread_cond_timedwait, in case of **missing a signal** that has been sent before and **spurious wakeups**.

4. We can send a **signal** (by calling pthread_cond_signal or pthread_cond_broadcast) without locking on the binding mutex, but **it's not safe**, a better way is requiring the mutex first.


#### cancellation point
A condition wait (whether timed or not) is a cancellation point.

When the cancelability type of a thread is set to PTHREAD_CANCEL_DEFERRED, a side-effect of acting upon a cancellation request while in a condition wait is that the mutex is (in effect) **re-acquired before** calling the first cancellation cleanup handler. The effect is as if the thread were unblocked, allowed to execute up to the point of returning from the call to pthread_cond_timedwait() or pthread_cond_wait(), but at that point notices the cancellation request and instead of returning to the caller of pthread_cond_timedwait() or pthread_cond_wait(), starts the thread cancellation activities, which includes calling cancellation cleanup handlers.

上面的文字来自posix文档，也就是说：条件变量是一个取消点，但是互斥量不是。因此pthread_cancel可以唤醒阻塞在条件变量上的等待线程，但是该线程随后重新获取mutex时，依然可能再次阻塞。总之，pthread_cancel不能保证一个等待线程能从 pthread_cond_wait/pthread_cond_timedwait 中返回。

#### mutually dependent relationship and shared_from_this
互相依赖的关系很容易因为未能保证正常的对象生命周期而导致内心泄漏。正如《POSIX多线程编程》中的crew示例(只摘取部分代码):
```C
typedef struct worker_tag {
    struct crew_tag     *crew;          /* Pointer to crew */
} worker_t, *worker_p;

/*
 * The external "handle" for a work crew. Contains the
 * crew synchronization state and staging area.
 */
typedef struct crew_tag {
    worker_t            crew[CREW_SIZE];/* Crew members */
} crew_t, *crew_p;
```
上面的代码设计了一个标识工作组的结构crew_t，以及工作组内的工作者（线程）worker_t。工作组结构crew_t要访问内部的工作者worker_t，而worker_t也需要访问crew_t的共享数据，如互斥量和条件变量等。

上面的代码在两个结构中包含了对方的裸指针。显然，这样的代码难以维护。这里没有说书中的代码不好的意思，只是想说明，C++有更好的方式。

以C++实现工作组时，可以通过weak_ptr来解耦这种强关联的互相引用：
```C
class Crew 
        /* 
        *   Crew cannot be constructed on stack any more. 
        *   So I make constructor protected. 
        */
        : public std::tr1::enable_shared_from_this<Crew> {

        friend class CrewProxy;

public:    
    class Worker {
    public:
        Worker(std::tr1::shared_ptr<Crew> crew, const int id);
        ~Worker();

    private:
        std::tr1::weak_ptr<Crew> crew_belongs_to_;          /* Pointer to crew */
    };

public:
    ~Crew();
protected:
    Crew();

private:
    std::vector<std::tr1::shared_ptr<Worker> > workers_;
};
```
1. 工作组Crew类有一个workers_成员，即工作组内的工作者，定义成std::tr1::shared_ptr<Worker>组成的vector。这样可以让工作者对象与Crew对象的生命周期关联起来，便于维护。

2. 工作者Worker定义一个std::tr1::weak_ptr<Crew>，即crew_belongs_to_，来访问Crew的共享数据。这个weak_ptr<Crew>对象crew_belongs_to_，不会影响工作者所属的Crew对象的生命周期（不会增加它的引用计数）。访问Crew对象时，通过crew_belongs_to_.expired()来判断Crew对象是否已经被释放，通过crew_belongs_to_.lock()把weak_ptr<Crew>转为shared_ptr<Crew>，以便访问Crew对象的共享数据。

3. Worker类的crew_belongs_to_成员是std::tr1::weak_ptr<Crew>，必须由std::tr1::shared_ptr<Crew>来初始化。也就是说，我们要把一个Crew对象的this变为std::tr1::shared_ptr<Crew>类型的对象。这可以通过继承std::tr1::enable_shared_from_this<Crew>来实现。通过继承std::tr1::enable_shared_from_this<Crew>,Crew对象可以通过shared_from_this()来获取包含该Crew对象的std::tr1::shared_ptr<Crew>对象。

4. **shared_from_this()**不会构造一个新的shared_ptr<T>, 它仅仅只是查找**现存的**包含this对象的那个shared_ptr<T>对象。因此，如果我们实现了shared_from_this()（通过继承std::tr1::enable_shared_from_this<Crew>），就不应该让其它代码能在栈上构造一个Crew对象，或者能创建一个指向Crew对象的裸指针，因为这些都可能导致shared_from_this()返回一个引用了已经被释放的对象的shared_ptr<Crew>对象。通常的做法是：类似上面的代码，把构造函数Crew()定义为protected,以便防止外部代码构造Crew对象。另外再定义一个friend class，比如上面的 **friend class CrewFactory**，作为一个工厂类，来创建Crew对象：

```C

//定义
class CrewFactory {
public:
    std::tr1::shared_ptr<Crew> GetCrew();

private:
std::tr1::shared_ptr<Crew> crew_;
};

//实现
std::tr1::shared_ptr<Crew> CrewFactory::GetCrew() {
    if (!crew_.get()) {
        crew_.reset(new Crew());
    }

    return crew_;
}
```