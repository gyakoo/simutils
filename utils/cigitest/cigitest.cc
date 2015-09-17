
#pragma warning(disable:4100 4005)

#if defined(_DEBUG) && !defined(_WIN64)
#include <vld.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <map>
#include <set>
#include <functional>
#include <mutex>
#include <atomic>
#include <deque>
#include <thread>
#include <vector>
#include <algorithm>

#define CIGI_VERSION_3_3
#define CIGI_IMPLEMENTATION
#include <cigi.h>

#define PIP_NOTCP
#define PIP_IMPLEMENTATION
#include <pip.h>


int main(int argc, const char** argv)
{
  bool host=false;
  bool ig=true;


  for ( int i = 1; i < argc; ++i )
  {
    if ( argv[i][0]=='-' )
    {
      switch ( argv[i][1] )
      {
        case 'r': break;
        case 'c': break;
      }
    }
  }

  return 0;
}