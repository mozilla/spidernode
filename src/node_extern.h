#ifndef SRC_NODE_EXTERN_H_
#define SRC_NODE_EXTERN_H_

#ifdef _WIN32
# ifndef BUILDING_NODE_EXTENSION
#   define NODE_EXTERN __declspec(dllexport)
# else
#   define NODE_EXTERN __declspec(dllimport)
# endif
#else
# define NODE_EXTERN /* nothing */
#endif

#endif  // SRC_NODE_EXTERN_H_
