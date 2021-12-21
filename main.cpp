#include <pthread.h>
#include <unistd.h>

#include <iostream>
#include <chrono>
#include <atomic>
#include <exception>
#include <thread>
#include <ctime>

const int num_of_runs = 8;
long long ** matrix;

enum State
{
  Preparing,
  Ready,
  Ping,
  Pong,
  Finish,
};

class Sync
{
public:
  State wait_as_long_as(State wait_state)
  {
    State loaded_state = state.load();
    while (loaded_state == wait_state)
      loaded_state = state.load();
    return loaded_state;
  }

  void wait_until(State expected_state)
  {
    while (state.load() != expected_state)
    {
    }
  }

  void set(State new_state)
  {
    state.store(new_state);
  }

private:
  std::atomic<State> state{Preparing};
};

static void set_affinity(unsigned int cpu_num)
{
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu_num, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

struct LatencyBench
{
  LatencyBench(long first_cpu_, long second_cpu_)
    : first_cpu{first_cpu_}
    , second_cpu{second_cpu_}
  {
  }

  long long operator()() const
  {
    Sync sync;

    set_affinity(first_cpu);

    std::thread t([&] {
      set_affinity(second_cpu);
      sync.set(Ready);

      State state = sync.wait_as_long_as(Ready);
      while (state != Finish)
      {
        if (state == Ping)
          sync.set(Pong);
        state = sync.wait_as_long_as(Pong);
      }
    });

    sync.wait_until(Ready);

	struct timespec ts, te;
    clock_getres(CLOCK_REALTIME, &ts);
    {
      sync.set(Ping);
      sync.wait_until(Pong);
    };
    clock_getres(CLOCK_REALTIME, &te);
    
    sync.set(Finish);
    t.join();
	return te.tv_nsec - ts.tv_nsec;
  }

  const long first_cpu;
  const long second_cpu;
};

int main()
{
	const long padding_cpus = 0;
  const long num_cpus = 8;// sysconf(_SC_NPROCESSORS_ONLN);
  
      matrix = new long long * [num_cpus];
    for (int i = 0; i < num_cpus; i++)
    {
        matrix[i] = new long long[num_cpus - 1];
    }

  for (long i = padding_cpus; i < num_cpus; ++i)
    for (long j = i + 1; j < num_cpus; ++j)
	{
		long long average = 0;
		for (int k = 0; k < num_of_runs; k++)
			average += LatencyBench(i, j)() / num_of_runs;
		matrix[i][j - 1] = matrix[j][i] = average;
	}
	
	for (int i = 0; i < num_cpus; i++)
    {
        for (int j = 0; j < num_cpus - 1; j++)
        {
            if (j == i)
                std::cout << ",";
            std::cout << matrix[i][j];
            if (j != num_cpus - 2)
                std::cout << ",";
        }
        if (i != num_cpus - 1)
            std::cout << std::endl;
    }
    std::cout << "," << std::endl;

    for (int i = 0; i < num_cpus; i++)
    {
        delete[] matrix[i];
    }
    delete[] matrix;
	
	return 0;
}
