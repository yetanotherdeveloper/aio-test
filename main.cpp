#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <unistd.h>
#include <memory>
#include <cstring>
#include <vector>
#include <x86intrin.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/aio_abi.h>


//#ifndef __rdtsc
//unsigned long long __rdtsc()
//{
  //return 1;
//}
//#endif

void mygemm()
{
  const int n = 100000;
  int input[n];
}

/// This function generated temporary file, opens it
// and fill with random integers then close it
int generate_data(char* filename_template, const unsigned int size_of_file, int num_chunks,int chunk_size,int num_elements_in_chunk ) 
{
  int fd = mkstemp(filename_template);

  if (fd == -1) {
    std::cerr << "Error opening/creating temporary file" << std::endl; 
    return(-1); 
  }   

  std::cout << "Generating File of random numbers of size: " << size_of_file << " ..."<< std::endl;  

  std::unique_ptr<int[]> bufek(new int[num_elements_in_chunk]); 

  for (unsigned int  i=0; i < num_chunks; ++i) {
    for (unsigned int j=0; j< num_elements_in_chunk;++j) {
      (bufek.get())[j] = i* j % 1024;  
    }
    write(fd, bufek.get(), chunk_size);
  }

  sync();

  close(fd);

  std::cout << "..done" << std::endl; 

  return fd;
}

void cleanup(char * filename_template)
{
  unlink(filename_template);
}

unsigned long long synchronous_read(char* filename_template, unsigned int size_of_file, int num_chunks,int chunk_size,int num_elements_in_chunk )
{

  int fd = open(filename_template,O_RDONLY);

  // Read Chunk , calculate hash (sumish)
  std::unique_ptr<int[]> bufek(new int[num_elements_in_chunk]); 

  long long int sumish = 0;

  auto t0 = __rdtsc();
 
  for(unsigned int i=0; i<num_chunks;++i) {

    read(fd, bufek.get(), chunk_size);

    for(unsigned int j=0; j< num_elements_in_chunk;++j) {
      sumish += (bufek.get())[j]; 
    }

  }

  auto t1 = __rdtsc();

  close(fd);

  return (t1 - t0);
}

unsigned long long asynchronous_read(char* filename_template, unsigned int size_of_file, int num_chunks,int chunk_size,int num_elements_in_chunk )
{
  aio_context_t ctx;
  struct iocb cb;
  struct iocb *cbs[1];
  struct io_event events[1];
  int ret;

  ctx = 0;  // This is requested by io_setup

  ret = syscall(__NR_io_setup, 128, &ctx);  // I had 2, but 128 was in tutorial

  if (ret < 0 ) {
    std::cerr << "Error creatin AIO context" << std::endl;
    return 0;
  }


  int fd = open(filename_template,O_RDONLY);

  // Read Chunk , calculate hash (sumish)
  std::unique_ptr<int[]> buf1(new int[num_elements_in_chunk]); 
  std::unique_ptr<int[]> buf2(new int[num_elements_in_chunk]); 


  char bufek[4096];

  // Queue with pointers
  // we will have a double buffer here eg.
  // read to one buffer and process another one
  std::vector<int*> buffers;
  buffers.push_back(buf1.get());
  buffers.push_back(buf2.get());

  long long int sumish = 0;
  int index = 0;

  auto t0 = __rdtsc();
 
  read(fd, buffers[index], chunk_size);
  index = 1 - index;  // Get next element (0 -> 1) or first one if we processed last one ( 1 -> 0)

  for(unsigned int i=0; i<num_chunks;++i) {

    // Initiate AIO read to buffers[index]
    memset(&cb,0,sizeof(cb));    
    cb.aio_fildes = fd;
    cb.aio_lio_opcode = IOCB_CMD_PREAD;
    cb.aio_buf = (uint64_t)(bufek);
    //cb.aio_buf = (uint64_t)(buffers[index]);
    //cb.aio_buf = (__u64)(buffers[index]);
    //cb.aio_offset = (i+1)*chunk_size;
    cb.aio_offset = 0;
    cb.aio_nbytes = 4096;

    cbs[0] = &cb;

    ret = syscall(__NR_io_submit, 1, cbs);  
    if (ret != 1) {
      if(ret < 0) {
        perror("Error: Submission of AIO failed!");
      }
      return 0;    
    }

    for(unsigned int j=0; j< num_elements_in_chunk;++j) {
      sumish += (buffers[1 - index])[j]; 
    }

    // Wait to see if data was read
    // wait indefinetly for one event
    ret = syscall(__NR_io_getevents,ctx, 1, 1, events, NULL);
    if (ret != 1) {
      if( ret < 0) {
        std::cerr << "Error: Events recieving of AIO failed!" << std::endl;
        return 0;    
      }
    }

    index = 1 - index;  // Get next element (0 -> 1) or first one if we processed last one ( 1 -> 0)
  }

  auto t1 = __rdtsc();

  ret = syscall(__NR_io_destroy, ctx);
  if (ret < 0 ) {
    std::cerr << "Error destroying AIO context" << std::endl;
    return 0;
  }

  close(fd);

  return (t1 - t0);
}


int main()
{

  char filename_template[] = "/tmp/AIOXXXXXX";   //< template from which we will have temporary file name generated
                                                 //< last six charace4trs need to be "X"
  const unsigned int size_of_file = 1024*1024*1024;
  int num_chunks = 1024;
  int chunk_size = size_of_file / num_chunks;
  int num_elements_in_chunk = chunk_size / sizeof(int);

  int fd = generate_data(filename_template,size_of_file,num_chunks,chunk_size,num_elements_in_chunk);

  auto asyn_time = asynchronous_read(filename_template, size_of_file,num_chunks,chunk_size,num_elements_in_chunk); 
  auto syn_time = synchronous_read(filename_template, size_of_file,num_chunks,chunk_size,num_elements_in_chunk); 

  const unsigned long tsc_freq = 2000000000;

  std::cout << "Synchronous Read and Compute: " <<  1000.0f*syn_time/(float)tsc_freq << " ms ("<< syn_time << ")"<< std::endl;
  std::cout << "Asynchronous Read and Compute: " <<  1000.0f*asyn_time/(float)tsc_freq << " ms ("<< asyn_time << ")"<< std::endl;

  cleanup(filename_template);

}
