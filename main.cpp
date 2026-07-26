#include "MatrixUI.hpp"
#include <vector>
#include <string>

void convert_vec(int argc , char const *argv[] , std::vector<std::string>& argv_std_vec){
  for(size_t i = 1 ; i < argc ; i++){
    argv_std_vec.push_back(std::string(argv[i]));
  }
}

int main(int argc, char const *argv[])
{
  std::vector<std::string> argv_std_vec;
  convert_vec(argc , argv , argv_std_vec);
  return compile(argv_std_vec);
}
