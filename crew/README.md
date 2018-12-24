
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