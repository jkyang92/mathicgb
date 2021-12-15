// MathicGB copyright 2012 all rights reserved. MathicGB comes with ABSOLUTELY
// NO WARRANTY and is licensed as GPL v2.0 or later - see LICENSE.txt.
#ifndef MATHICGB_M_TBB_GUARD
#define MATHICGB_M_TBB_GUARD

// TODO: Have cmake and autotools set this variable, as well as TBB_VERSION_STRING

#define MTBB_VERSION 2021
//#define MTBB_VERSION 0 // TBB not present
//#define MTBB_VERSION 2020

#if MTBB_VERSION>=2021
  #include <tbb/version.h> // only works for tbb2021
#elif MTBB_VERSION>0
  #define mtbbstringize0(a) #a
  #define mtbbstringize(a) mtbbstringize0(a)
  #include <tbb/tbb_stddef.h> // only works for tbb2020 and older, we think
  #define TBB_VERSION_STRING "2020.3" //  todo! get the next line to work!
//    (mtbbstringize(TBB_VERSION_MAJOR) "." mtbbstringize(TBB_VERSION_MINOR))
#else
  #define TBB_VERSION_STRING "TBB not present"
#endif

/// A compatibility layer for tbb. If we are compiling with tbb present, then
/// these classes will simply be the same classes as in tbb. However, if we
/// are compiling without tbb (so without parallelism), then these classes will
/// be trivial non-parallel implementations that allows MathicGB to work
/// without tbb being present. TBB doesn't work on Cygwin, so that is at least
/// one good reason to have this compatibility layer. This only works if all
/// uses of tbb go through mtbb, so make sure to do that.

#if MTBB_VERSION>0 // TBB present
 
#if MTBB_VERSION>=2021
  #include <tbb/global_control.h>
  #include <tbb/info.h>  
  #include <tbb/parallel_for_each.h>
#else
  #include <tbb/task_scheduler_init.h>
  #include <tbb/parallel_do.h>
#endif
#include <tbb/enumerable_thread_specific.h>
#include <tbb/concurrent_unordered_map.h>     
#include <tbb/queuing_mutex.h>                // for queuing_mutex
#include <tbb/null_mutex.h>                   // for null_mutex
#include <tbb/tick_count.h>                   // for tick_count
#include <tbb/parallel_sort.h>                // for parallel_sort
#include <tbb/parallel_for.h>                 // for parallel_for
#include <mutex>

namespace mtbb {
  using ::tbb::enumerable_thread_specific;
  using ::tbb::queuing_mutex;
  using ::tbb::null_mutex;
  using ::tbb::parallel_sort;
  using ::tbb::parallel_for;
  using ::tbb::blocked_range;
  using ::tbb::tick_count;
  using ::std::mutex;
  using lock_guard = ::std::lock_guard<std::mutex>;

  template<class Key, class T, class Hash, class KeyEqual>
  using unordered_map = ::tbb::concurrent_unordered_map<Key, T, Hash, KeyEqual>;
  
  template<typename T>
#if MTBB_VERSION >= 2021
  using feeder = ::tbb::feeder<T>;
#else
  using feeder = ::tbb::parallel_do_feeder<T>;
#endif
    
  template<typename T1, typename T2>
  static inline void parallel_for_each(T1 a, T1 b, T2 c)
  {
#if MTBB_VERSION >= 2021
    tbb::parallel_for_each(a,b,c);
#else
    tbb::parallel_do(a,b,c);
#endif
  }

  class task_scheduler_init {
  public:
    task_scheduler_init(int nthreads) {
      const auto tbbMaxThreadCount = nthreads == 0 ?
#if MTBB_VERSION >= 2021
        tbb::info::default_concurrency() : nthreads;
      tbb::global_control global_limit(tbb::global_control::max_allowed_parallelism,
                                       tbbMaxThreadCount);
#else
        ::tbb::task_scheduler_init::automatic : nthreads;
      ::tbb::task_scheduler_init scheduler(tbbMaxThreadCount);
#endif
    }
  };
}

#else // TBB not present

// below is an interface to serial versions of the above code.
#include <unordered_map>
#include <functional>
#include <vector>
#include <ctime>
#include <algorithm>
#include <chrono>

namespace mtbb {

  class task_scheduler_init {
  public:
    task_scheduler_init(int) {}
    static const int automatic = 1;
  };

  class mutex {
  public:
    mutex(): mLocked(false) {}

    void lock() {
      assert(!mLocked); // deadlock
      mLocked = true;
    }

    bool try_lock() {
      if (mLocked)
        return false;
      lock();
      return true;
    }

    void unlock() {
      assert(mLocked);
      mLocked = false;
    }

    class scoped_lock {
    public:
      scoped_lock(): mMutex(0) {}
      scoped_lock(mutex& m): mMutex(&m) {mMutex->lock();}
      ~scoped_lock() {
        if (mMutex != 0)
          release();
      }
      
      void acquire(mutex& m) {
        assert(mMutex == 0);
        mMutex = &m;
      }
      
      bool try_acquire(mutex& m) {
        assert(mMutex == 0);
        if (!m.try_lock())
          return false;
        mMutex = &m;
        return true;
      }
      
      void release() {
        assert(mMutex != 0);
        mMutex->unlock();
        mMutex = 0;
      }
      
    private:
      mutex* mMutex;
    };
    
  private:
    bool mLocked;
  };

  using lock_guard = mutex::scoped_lock;

  using null_mutex = mutex;
  using queuing_mutex = mutex;

  template<class Key, class T, class Hash, class KeyEqual>
  using unordered_map = ::std::unordered_map<Key, T, Hash, KeyEqual>;
  
  template<class T>
  class enumerable_thread_specific {
  public:

    template<class Op>
    enumerable_thread_specific(Op&& creater): mCreater(creater) {}

    bool empty() const {return mObj.get() == 0;}

    using reference = T&;
      
    T& local() {
      if (empty())
        mObj = std::make_unique<T>(mCreater());
      assert(!empty());
      return *mObj;
    }

    T* begin() {
      if (empty())
        return 0;
      else
        return mObj.get();
    }

    T* end() {
      if (empty())
        return  0;
      else
        return begin() + 1;
    }

    void clear() {
      mObj.reset(0);
    }

  private:
    std::function<T()> mCreater;
    std::unique_ptr<T> mObj;
  };
  
  template<class Value>
  class blocked_range {
  public:
    typedef size_t size_type;
    typedef Value const_iterator;

    blocked_range(Value begin, Value end, size_t grainSize = 1):
      mBegin(begin), mEnd(end), mGrainSize(grainSize) {}

    size_type size() const {return end() - begin();}
    bool empty() const {return mBegin == mEnd;}
    size_type grainsize() const {return mGrainSize;}
    bool is_divisible() const {return false;}

    const_iterator begin() const {return mBegin;}
    const_iterator end() const {return mEnd;}

  private:
    const_iterator mBegin;
    const_iterator mEnd;
    size_type mGrainSize;
  };
    
  template<class Range, class Func>
  void parallel_for(Range&& range, Func&& f) {
    f(range);
  }

  template<class Index, class Func>
  void parallel_for(Index begin, Index end, Index step, Func&& f) {
    for (auto i = begin; i < end; i += step)
      f(i);
  }

  template<class T>
  class feeder {
  public:
    feeder(std::vector<T>& tasks): mTasks(tasks) {}

    template<class TT>
    void add(TT&& t) {mTasks.push_back(std::forward<TT>(t));}

  private:
    std::vector<T>& mTasks;
  };

  template<class InputIterator, class Body>
  void parallel_for_each(InputIterator begin, InputIterator end, Body body) {
    typedef typename std::remove_reference<decltype(*begin)>::type Task;
    std::vector<Task> tasks;
    feeder<Task> feeder(tasks);
    for (; begin != end; ++begin) {
      tasks.push_back(*begin);
      while (!tasks.empty()) {
        auto task = std::move(tasks.back());
        tasks.pop_back();
        body(task, feeder);
      }
    }
  }

  template<class It, class Pred>
  void parallel_sort(It begin, It end, Pred&& pred) {
    std::sort(begin, end, pred);
  }

  class tick_count {
  private:
    // This really should be std::chrono::steady_clock, but GCC 4.5.3 doesn't
    // have that.
    typedef std::chrono::system_clock clock;

  public:
    tick_count(): mTime() {}

    static tick_count now() {
      tick_count t;
      t.mTime = clock::now();
      return t;
    }

    class interval_t {
    public:
      interval_t(double seconds): mSeconds(seconds) {}

      double seconds() const {return mSeconds;}

    private:
      const double mSeconds;
    };

    interval_t operator-(const tick_count t) const {
      typedef std::chrono::duration<double> SecondDuration;
      const auto duration =
        std::chrono::duration_cast<SecondDuration>(mTime - t.mTime);
      return duration.count();
    }

  private:
    clock::time_point mTime;
  };
}

#endif
#endif
