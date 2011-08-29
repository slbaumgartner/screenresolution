/*
 * screenresolution
 * Set the screen resolution for the main display from the command line
 *
 * Build: clang -framework ApplicationServices main.c -o screenresolution
 *
 * John Ford <john@johnford.info>
 */

#include <ApplicationServices/ApplicationServices.h>
#include "version.h"

// Don't know how realistic it is to have more than 10 at this point.
// Feel free to remind me about 640k being enough :)
#define MAX_DISPLAYS 10

//Number of modes to list per line
#define MODES_PER_LINE 4

struct config{
    size_t w;
    size_t h;
    size_t d;
};

unsigned int setDisplayToMode (CGDirectDisplayID display, CGDisplayModeRef mode);
unsigned int configureDisplay(CGDirectDisplayID display, struct config * config, int displayNum);
unsigned int listCurrentMode(CGDirectDisplayID display, int displayNum);
unsigned int listAvailableModes(CGDirectDisplayID display, int displayNum);
unsigned int parseStringConfig(const char* string, struct config * out);
size_t bitDepth(CGDisplayModeRef mode);


int main (int argc, const char * argv[]) {
    unsigned int exitcode = 0; 

    if (argc > 1) {
        CGError rc;
        uint32_t displayCount;
        CGDirectDisplayID activeDisplays [MAX_DISPLAYS];
        int d;
        rc = CGGetActiveDisplayList(MAX_DISPLAYS, activeDisplays, &displayCount);
        if (rc != kCGErrorSuccess) {
            fprintf(stderr, "Error: failed to get list of active displays");
            exitcode++;
        }
        int keepgoing = 1;
        // This loop should probably be in another function
        for (d = 0; d < displayCount && keepgoing; d++){
            if (strcmp(argv[1], "get") == 0) {
                if (!listCurrentMode(activeDisplays[d], d)){
                    exitcode++;
                }
            } else if (strcmp(argv[1], "list") == 0) {
                if (!listAvailableModes(activeDisplays[d], d)){
                    exitcode++;
                }
            } else if (strcmp(argv[1], "set") == 0) {
                if (strcmp(argv[d+2], "skip") == 0) {
                    printf("Skipping display %d\n", d);
                } else {
                    struct config newConfig;
                    if (parseStringConfig(argv[d+2], &newConfig)) {
                        if (!configureDisplay(activeDisplays[d], &newConfig, d)){
                            exitcode++;
                        }
                    } else {
                        exitcode++;
                    }
                }
            } else if (strcmp(argv[1], "-version") == 0) {
                printf("screenresolution version %s\n", VERSION);
                keepgoing = 0;
            } else {
                fprintf(stderr, "I'm sorry %s.  I'm affraid I can't do that\n", getlogin());   
                exitcode++;
                keepgoing = 0;
            }
        }
    } else {
        fprintf(stderr, "why failed? because usage\n");
        exitcode++;
    }
    return exitcode > 0;
}

size_t bitDepth(CGDisplayModeRef mode) {
    size_t depth = 0;
	CFStringRef pixelEncoding = CGDisplayModeCopyPixelEncoding(mode);
	if (kCFCompareEqualTo == CFStringCompare(pixelEncoding, CFSTR(IO32BitDirectPixels), kCFCompareCaseInsensitive)) {
		depth = 32;
    }
	else if (kCFCompareEqualTo == CFStringCompare(pixelEncoding, CFSTR(IO16BitDirectPixels), kCFCompareCaseInsensitive)) {
		depth = 16;
    }
	else if (kCFCompareEqualTo == CFStringCompare(pixelEncoding, CFSTR(IO8BitIndexedPixels), kCFCompareCaseInsensitive)) {
		depth = 8;
    }
	return depth;
}

unsigned int configureDisplay(CGDirectDisplayID display, struct config * config, int displayNum){
    unsigned int returncode = 1;
    CFArrayRef allModes = CGDisplayCopyAllDisplayModes(display, NULL);
    if (allModes == NULL) {
        fprintf(stderr, "Error: failed trying to look up modes for display %d", displayNum);
    }
    CGDisplayModeRef newMode = NULL;
    size_t pw; // possible width
    size_t ph; // possible height
    size_t pd; // possible depth
    int looking = 1; // used to decide whether to continue looking for modes
    int i;
    for (i = 0 ; i < CFArrayGetCount(allModes) && looking ; i++) {
        CGDisplayModeRef possibleMode = (CGDisplayModeRef) CFArrayGetValueAtIndex(allModes, i);
        pw = CGDisplayModeGetWidth(possibleMode);
        ph = CGDisplayModeGetHeight(possibleMode);
        pd = bitDepth(possibleMode);
        if ( pw == config->w && ph == config->h && pd == config->d ) {
            looking = 0; // Stop looking for more modes!
            newMode = possibleMode;
        }
    }
    if (newMode != NULL) {
        printf("Setting mode on display %d to %lux%lux%lu\n", displayNum, pw,ph,pd);
        setDisplayToMode(display,newMode);
    } else {
        fprintf(stderr, "Error: requested mode (%lux%lux%lu) is not available\n", config->w, config->h, config->d);
        returncode = 0;
    }
    return returncode;
}


unsigned int setDisplayToMode (CGDirectDisplayID display, CGDisplayModeRef mode) {
    unsigned int returncode = 1;
    CGError rc;
    CGDisplayConfigRef config;
    rc = CGBeginDisplayConfiguration(&config);
    if (rc != kCGErrorSuccess) {
        fprintf(stderr, "Error: failed CGBeginDisplayConfiguration");
        returncode = 0;
    }
    if (returncode){
        rc = CGConfigureDisplayWithDisplayMode(config, display, mode, NULL);
        if (rc != kCGErrorSuccess) {
            fprintf(stderr, "Error: failed CGConfigureDisplayWithDisplayMode");
            returncode = 0;
        }
    }
    if (returncode){
        rc = CGCompleteDisplayConfiguration(config, kCGConfigureForSession);
        if (rc != kCGErrorSuccess) {
            fprintf(stderr, "Error: failed CGCompleteDisplayConfiguration");
            returncode = 0;
        }
    }
    return returncode;
}
unsigned int listCurrentMode(CGDirectDisplayID display, int displayNum){
    unsigned int returncode = 1;
    CGDisplayModeRef currentMode = CGDisplayCopyDisplayMode(display);
    if (currentMode == NULL) {
        fprintf(stderr, "Error: unable to copy current display mode");
        returncode = 0;
    }
    printf("Display %d: %lux%lux%lu\n", 
           displayNum,
           CGDisplayModeGetWidth(currentMode), 
           CGDisplayModeGetHeight(currentMode),
           bitDepth(currentMode));
    CGDisplayModeRelease(currentMode);
    return returncode;
}

unsigned int listAvailableModes(CGDirectDisplayID display, int displayNum){
    unsigned int returncode = 1;
    int i;
    CFArrayRef allModes = CGDisplayCopyAllDisplayModes(display, NULL);
    if (allModes == NULL) {
        returncode = 0;
    }
    printf("Available Modes on Display %d\n", displayNum);
    for (i = 0 ; i < CFArrayGetCount(allModes) && returncode ; i++) {
        CGDisplayModeRef mode = (CGDisplayModeRef) CFArrayGetValueAtIndex(allModes, i);
        //This formatting is functional but it ought to be done less poorly
        if (i % MODES_PER_LINE == 0) {
            printf("  ");
        } else {
            printf("\t");
        }
        printf("%lux%lux%lu ",
               CGDisplayModeGetWidth(mode),
               CGDisplayModeGetHeight(mode),
               bitDepth(mode));
        if (i % MODES_PER_LINE == MODES_PER_LINE - 1) {
            printf("\n");
        }
    }
    printf("\n");
    return returncode;
}

unsigned int parseStringConfig(const char* string, struct config * out){
    unsigned int rc;
    size_t w;
    size_t h;
    size_t d;
    int numConverted = sscanf(string, "%lux%lux%lu", &w, &h, &d);
    if (numConverted != 3) {
        rc = 0;
        fprintf(stderr, "Error: the mode '%s' couldn't be parsed", string);
    } else{
        out->w = w;
        out->h = h;
        out->d = d;
        rc = 1;
    }
    return rc;
}
