
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