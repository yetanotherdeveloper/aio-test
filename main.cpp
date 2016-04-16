#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <memory>
#include <x86intrin.h>
#include <sys/stat.h>
#include <fcntl.h>

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

unsigned long long synchronouse_read(char* filename_template, unsigned int size_of_file, int num_chunks,int chunk_size,int num_elements_in_chunk )
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

int main()
{

  char filename_template[] = "/tmp/AIOXXXXXX";   //< template from which we will have temporary file name generated
                                                 //< last six charace4trs need to be "X"
  const unsigned int size_of_file = 1024*1024*1024;
  int num_chunks = 1024;
  int chunk_size = size_of_file / num_chunks;
  int num_elements_in_chunk = chunk_size / sizeof(int);

  int fd = generate_data(filename_template,size_of_file,num_chunks,chunk_size,num_elements_in_chunk);

  auto syn_time = synchronouse_read(filename_template, size_of_file,num_chunks,chunk_size,num_elements_in_chunk); 

  const unsigned long tsc_freq = 2000000000;

  std::cout << "Synchronous Read and Compute: " <<  1000.0f*syn_time/(float)tsc_freq << " ms ("<< syn_time << ")"<< std::endl;

  cleanup(filename_template);

}
