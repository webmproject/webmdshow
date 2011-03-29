#ifndef ON2_PORTS_MEM_H
#define ON2_PORTS_MEM_H

#if defined(__GNUC__) && __GNUC__
  #define DECLARE_ALIGNED(n,typ,val)  typ val __attribute__ ((aligned (n)))
#elif defined(_MSC_VER)
  #define DECLARE_ALIGNED(n,typ,val)  __declspec(align(n)) typ val
#else
  #warning No alignment directives known for this compiler.
  #define DECLARE_ALIGNED(n,typ,val)  typ val
#endif
#endif
