#include "imagecore/imagecore.h"

static ImageCoreAssertionHandler s_AssertionHandler;

static void DefaultAssertionHandler(int code, const char* message, const char* file, int line)
{
	fprintf(stderr, "assertion failed: %s (%s:%i)", message, file, line);
#if DEBUG
	__builtin_trap();
#else
	exit(code);
#endif
}

void ImageCoreAssert(int code, const char* message, const char* file, int line)
{
	if( s_AssertionHandler == NULL ) {
		DefaultAssertionHandler(code, message, file, line);
	} else {
		s_AssertionHandler(code, message, file, line);
	}
}

void RegisterImageCoreAssertionHandler(ImageCoreAssertionHandler handler)
{
	s_AssertionHandler = handler;
}

#if __APPLE__

#include <mach/mach_time.h>

double ImageCoreGetTimeMs()
{
	static mach_timebase_info_data_t sTimebaseInfo;
	if ( sTimebaseInfo.denom == 0 ) {
		(void) mach_timebase_info(&sTimebaseInfo);
	}
	return (double)((mach_absolute_time() * sTimebaseInfo.numer / sTimebaseInfo.denom) / 1000) / 1000.0;
}

#else

double ImageCoreGetTimeMs()
{
	return (double)clock() / (double)(CLOCKS_PER_SEC / 1000);
}

#endif

