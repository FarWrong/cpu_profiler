#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>
using namespace std::chrono;

//variables to set for testing
int num_threads = 4;
int sleep_length = 1;
int profile_time = 100;

//initialize global variables
int initialized = 0;
std::chrono::time_point<std::chrono::_V2::system_clock, std::chrono::_V2::system_clock::duration> endtime;
int min_prio_for_policy;
void* sleep_thread(void * arg);
void* run_computation(void * arg);
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;

//Arguments for each thread
struct thread_args {
  int id;
  int start_time = 0;
  int end_time = 0;
  pthread_mutex_t mutex;
};

//To get steal time of a CPU
int get_steal_time(int cpunum){
  std::ifstream f("/proc/stat");
  std::string s;
  for (int i = 0; i <= cpunum + 1; i++){
        std::getline(f, s);
  }
  unsigned n;
  std::string l;
  if(std::istringstream(s)>> l >> n >> n >> n >> n >> n >> n >>n >> n )
    {
        return(n);
    }

    return 0;
}

//To get steal time of ALL CPUs
void get_steal_time_all(int cpunum,int steal_arr[]){
  std::ifstream f("/proc/stat");
  std::string s;
  int output[cpunum];
  
  std::getline(f, s);
  for (int i = 0; i < cpunum; i++){
        std::getline(f, s);
        unsigned n;
        std::string l;
        if(std::istringstream(s)>> l >> n >> n >> n >> n >> n >> n >>n >> n )
        {
        // use n here...
        steal_arr[i] = n;
        }
  }
}

//get run time of ALL cpus
void get_run_time_all(int cpunum,int run_arr[]){
  std::ifstream f("/proc/stat");
  std::string s;
  int output[cpunum];
  
  std::getline(f, s);
  for (int i = 0; i < cpunum; i++){
        std::getline(f, s);
        unsigned n;
        std::string l;
        if(std::istringstream(s)>> l >> n )
        {
        // use n here...
        run_arr[i] = n;
        }
  }
}

int main() 
{
  //get local CPUSET
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);

  //intialize mutex, threads, and stealtime + runtime trackers
  pthread_t thread_array[num_threads];
  pthread_mutex_t mutex_array[num_threads];
  struct thread_args* args_array[num_threads];
  int steal_time_end[num_threads];
  int steal_time_begin[num_threads];


  int run_time_end[num_threads];
  int run_time_begin[num_threads];

  pthread_t thId = pthread_self();
  pthread_attr_t thAttr;
  
  //Fetch highest and lowest possible prios(and set current thread to highest)
  int policy = 0;
  int max_prio_for_policy = 0;
  pthread_attr_init(&thAttr);
  pthread_attr_getschedpolicy(&thAttr, &policy);
  max_prio_for_policy = sched_get_priority_max(policy);
  min_prio_for_policy = sched_get_priority_min(policy);
  pthread_setschedprio(thId, max_prio_for_policy);
  pthread_attr_destroy(&thAttr);


  //create all the threads and initilize mutex
  for (int i = 0; i < num_threads; i++) {
    struct thread_args *args = new struct thread_args;
    
    //init mutex
    mutex_array[i] =  PTHREAD_MUTEX_INITIALIZER;
    //decide which cores to bind cpus too
    CPU_SET(i , &cpuset);
    //give an id and assign mutex to all threads
    args->id = i;
    args->mutex = mutex_array[i];
    //set prio of thread to MIN
    
    pthread_create(&thread_array[i], NULL, run_computation, (void *) args);
    pthread_setaffinity_np(thread_array[i], sizeof(cpu_set_t), &cpuset);

    args_array[i] = args;
  
  }


  //start profiling+resting loop
  while(true){

    //sleep for sleep_length
    printf("sleeping \n");
    sleep(sleep_length);
    
    //wake up and start profiling
    printf("profiling \n");
    
    int starttime = duration_cast< milliseconds >(system_clock::now().time_since_epoch()).count() % 10000; 
    endtime = high_resolution_clock::now() + std::chrono::milliseconds(profile_time);

    //wake up threads and broadcast 
    initialized = 1;
    pthread_cond_broadcast(&cv);
    get_steal_time_all(num_threads,steal_time_begin);
    get_run_time_all(num_threads,run_time_begin);
    //Wait for processors to finish profiling
    while(std::chrono::high_resolution_clock::now() < endtime){
    }
    get_steal_time_all(num_threads,steal_time_end);
    get_run_time_all(num_threads,run_time_end);
    auto end = duration_cast< milliseconds >(system_clock::now().time_since_epoch()).count() % 10000; 
    for (int i = 0; i < num_threads; i++) {
      int stolentime = steal_time_end[i]-steal_time_begin[i];
      int rantime = run_time_end[i]-run_time_begin[i];
      std::cout<< "Thread:"<<args_array[i]->id << " Steal Time:" << stolentime <<" User CPU Time:"<<rantime<<" Start Time:"<< args_array[i]->start_time<< " Start Time Main:" << starttime << " End Time:" << args_array[i]->end_time  % 10000<< " End Time Main:" << end << std::endl ;
    };
  }

  //join the threads
  for (int i = 0; i < num_threads; i++) {
    pthread_join(thread_array[i], NULL);
  }
  printf("Process Finished");
  return 0;
}



int get_profile_time(int cpunum){
  std::ifstream f("/proc/stat");
  std::string s;
  for (int i = 0; i <= cpunum; i++){
        std::getline(f, s);
  }
  unsigned n;
  std::string l;
  if(std::istringstream(s)>> l >> n )
    {
        // use n here...
        return(n);
    }

    return 0;
}

void* run_computation(void * arg)
{

    struct thread_args *args = (struct thread_args *)arg;
    while(true){
      pthread_mutex_lock(&args->mutex);
      
      while (! initialized) {
      pthread_cond_wait(&cv, &args->mutex);
      }
      pthread_mutex_unlock(&args->mutex);
      
      int addition_calculator = 0;


      //int start_steal = get_steal_time(args->id);
      //int start_computation = get_profile_time(args->id);

      //int ms = duration_cast< milliseconds >(system_clock::now().time_since_epoch()).count();
      args->start_time = duration_cast< milliseconds >(system_clock::now().time_since_epoch()).count() % 10000;

      while(std::chrono::high_resolution_clock::now() < endtime &&  initialized){
        
        addition_calculator += 1;
      };
      args->end_time = duration_cast< milliseconds >(system_clock::now().time_since_epoch()).count() % 10000;
     // ms = duration_cast< milliseconds >(system_clock::now().time_since_epoch()).count();
      initialized = 0;
      }
      return NULL;
} 