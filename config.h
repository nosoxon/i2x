#pragma once

#define DEBUG   0
#define VERSION "unknown"

#ifdef DEBUG
#define dbprintf(args...)       printf(args)
#else
#define dbprintf(args...)	{}
#endif
