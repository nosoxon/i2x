#pragma once

#define DUMMY_IOCTL 0
#define DEBUG 1
#define VERSION "unknown"

#if DEBUG == 1
#define dbprintf(args...)       printf(args)
#else
#define dbprintf(args...)	{}
#endif
