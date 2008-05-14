/*
   joy2key 
   
   This program gets events from the joystick device /dev/input/js0 and
   dispatches X events or console keypresses to a specific window
   based on joystick status.  It is most useful for adding joystick
   support to games that don't nativly support the joystick, using
   joy2key to send XKeyPress (or console ioctl()) messages in place of
   actually typing movement keys.  This code is under the GNU Public
   License (see COPYING), you can do whatever you want to it, and if
   you do anything really neat, I'd like to see it!

		- Peter Amstutz (tetron@interreality.org)

		special thanks to Zhivago of EFNet's #c for helping me out 
		a LOT with the XSendEvent() code...

		The latest version of joy2key can be found in 
			ftp://sunsite.unc.edu/pub/Linux/X11/xutils
		or on joy2key's (rather spartan) website,
			http://www-unix.oit.umass.edu/~tetron/joy2key

		Revision History
		----------------
        1.0 (12 Aug 1997) - First version.
        1.1 (04 Dec 1997) - Thanks to Craig Lamparter 
		                    (craiger@hemna.rose.hp.com), 
		                    joy2key now has support for four-buttoned 
                            joysticks/gamepads!  Also some bugfixes.
        1.2 (28 Apr 1998) - Pierre Juhen (pierre.juhen@wanadoo.fr) 
		                    fixed a dumb little bug and added the 
                            autorepeat feature!  
        1.3 (04 Jun 1998) - Frederic Lepied (fred@sugix.frmug.org) added 
		                    option to call w/o the window title arg to 
                            select target window interactivly 
                            (gee, why didn't I do that in the first 
                            place?) and detect when the window closed and
                            exit properly.  Also now uses 1.0 joystick 
                            module event-based handling instead of 
                            polling (now _requires_ 1.0+ joystick driver,
                            which is available for v2.0+ Linux kernels),
                            and is able to send events to the console 
                            with an ioctl() as well!
                            Lots of code cleanup and restructuring.
        1.4 (17 Aug 1998) - .joy2keyrc implemented, this adds -rcfile and 
                            -config.  Bugfix for -dev command.  Also 
                            automatic calibration when no -thresh is found, 
                            added -deadzone option.
        1.5 (20 Jul 1999) - Patch by Jeremy (jeremy@xxedgexx.com) to look up 
                            windows by their numeric ID when they cannot by 
                            found by name; this makes it possible to use 
                            joy2key with full-screen (DGA) X apps.
                            Joy2key also now uses autoconf/automake.
	1.6 (10 Apr 2001) - Okay, after getting mail about it for
                            literally years, I've finally fixed the
			    bug parsing joy2keyrc files...  ;-p
	1.6.1 (27 Jan 2005) - Just a trivial update to fix the web page
	                    and email addresses.
	1.6.2 (22 Mar 2007) Rens (mraix@xs4all.nl)
                          - Fixed a bug, preventing joy2key from using your
                            highest numbered button.
                          - Made joy2key lower case / upper case - aware
                            by simulating actual keyboard behaviour: capital
                            A is generated like this:
                            key-shift-down, key-"a"-down, key-"a"-up, key-shift-up
                            Exactly as you would type it..... It sounds trivial,
                            but I found out that a lot of games
                            ( Especially Windoze games running in Wine )
                            require exact simulation of the keyboard-presses
                            or there will be no joy(2key).
	1.6.3 (12 Apr 2008) Peter Alfredsen (peter.alfredsen@gmail.com)
                          - Fix up a few compiler warnings.
                          - Once and for all fix configure.ac so it works.
                          - Merge Debian, OpenSuse and Gentoo patches.
                            Related bugs:
                              Opensuse:
                                https://bugzilla.novell.com/226508
                              Gentoo:
                                http://bugs.gentoo.org/82685
                              Debian:
                                http://bugs.debian.org/365146
                                http://bugs.debian.org/404543
                                http://bugs.debian.org/404550
                          - Fix up the ... unique ... coding style that Rens
                            Introduced. Merge only relevant parts of his changes.
                            Change comment style to C.
*/

/* Should be specified in the makefile */
/* #define ENABLE_X */
/* #define ENABLE_CONSOLE */

#include "config.h"

#define JOY2KEY_VERSION                VERSION

#define DEFAULT_AUTOREPEAT             5
#define DEFAULT_DEADZONE               50
#define DEFAULT_DEVICE                 "/dev/input/js0"
#define DEFAULT_RCFILE                 ".joy2keyrc" /* located in $(HOME) */
#define PETERS_EMAIL                   "tetron@interreality.org"

#define XK_MISCELLANY 			1
#define XK_LATIN1 			1
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/joystick.h>

#ifdef ENABLE_X
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#endif

int unsigned button_actions[256];
int button_upper[256];
int jsfd=-1,
    axis_actions[256][2],
    axis_threshold[256][2], /* 0 = low (down); 1 = hi (up) */
    axis_threshold_defined[256], /* 1 == defined */
    axis_act_counter=0,
    button_act_counter=0,
    thresh_counter=0,
    deadzone=DEFAULT_DEADZONE;
char *device=DEFAULT_DEVICE, 
    *rcfile=DEFAULT_RCFILE, 
    button_repeat_flags[256],
    common_read=0;
typedef enum {NONE, X, RAWCONSOLE, TERMINAL} target_type;
typedef enum {PRESS, RELEASE} press_or_release_type;
struct itimerval repeat_time;

target_type target=NONE;
#ifndef ENABLE_X
#ifndef ENABLE_CONSOLE
#error No targets defined!
#endif
#endif

#ifdef ENABLE_X
Display *thedisp;
int thewindow=0, 
    thescreen=0;
static Window RegisterCloseEvent(Display *disp, Window win);
static void CheckIfWindowClosed(Display	*disp, Window parentwin, Window win);
#endif

#ifdef ENABLE_CONSOLE
int consolefd;
#endif

void process_args(int argc, char **argv);
int check_config(int argc, char **argv);
int check_device(int argc, char **argv);
void sendkey(unsigned int keycode, press_or_release_type PoR, int iscap);
void cleanup(int s);
void repeat_handler(int s);
void calibrate(int num);
int argtokey(char *arg);

int main(int argc, char **argv)
{
#ifdef ENABLE_X
    int parentwin=0;
    fd_set close_set;
#endif
    FILE *foo;
    char string[255],
		axis_hold_flags[256],
		numaxes, 
		numbuttons;
    struct js_event js;
    int i;
    struct timeval tv;
    
    puts("joy2key - reads joystick status and dispatches keyboard events");
    puts("By Peter Amstutz ("PETERS_EMAIL")");
    puts("This is free software under the GNU General Public License (GPL v2)");
    puts("              (see COPYING in the joy2key archive)");
    puts("You are welcome to use/modify this code, and please e-mail me");
    puts("if anything cool comes of it!");
    printf("Version: %s   Binary built on %s at %s\n\n", 
		   JOY2KEY_VERSION, __DATE__, __TIME__);

    memset(axis_threshold, 0, sizeof(axis_threshold));
    memset(axis_threshold_defined, 0, sizeof(axis_threshold_defined));
    memset(axis_actions, 0, sizeof(axis_actions));
    memset(button_actions, 0, sizeof(button_actions));
    memset(button_upper, 0, sizeof(button_upper));
    memset(button_repeat_flags, 0, sizeof(button_repeat_flags));
    memset(axis_hold_flags, 0, sizeof(axis_hold_flags));
    repeat_time.it_interval.tv_sec=0;
    repeat_time.it_interval.tv_usec=0;
    repeat_time.it_value.tv_sec=0;
    repeat_time.it_value.tv_usec=0;

    argc=check_config(argc, argv);
    process_args(argc, argv);

    if((jsfd=open(device,O_RDONLY))==-1)
    {
		printf("Error opening %s!\n", device);
		puts("Are you sure you have joystick support in your kernel?");
		return 1;
    }
    if (ioctl(jsfd, JSIOCGAXES, &numaxes)) {
/* acording to the American Heritage Dictionary of the English 
   Language 'axes' *IS* the correct pluralization of 'axis' */
		perror("joy2key: error getting axes"); 
		return 1;
    }
    if (ioctl(jsfd, JSIOCGBUTTONS, &numbuttons)) {
		perror("joy2key: error getting buttons");
		return 1;
    }

    if(numaxes<axis_act_counter) puts("More axes specificed than joystick has!");
    if(numbuttons<button_act_counter) puts("More buttons specificed than joystick has!");

    switch(target)
    {
#ifdef ENABLE_X	
    case X:
		thescreen=DefaultScreen(thedisp);
		if (argc == 1 || argv[1][0] == '-') {
			{ 
				puts("Please select a window to send events to");
				sprintf(string, "xwininfo");
			}
		} else {
			sprintf(string, "xwininfo -name \"%s\"", argv[1]);
		}
		foo=popen(string, "r");
		while(!feof(foo) && fgets(string, 255, foo) &&
			  (sscanf(string, "xwininfo: Window id: %x", &thewindow) != 1));
		pclose(foo);

		if(thewindow==0) 
		{
			sprintf(string, "xwininfo -id \"%s\"", argv[1]);
			foo=popen(string, "r");
			while(!feof(foo) && fgets(string, 255, foo) &&
				  (sscanf(string, "xwininfo: Window id: %x", &thewindow) != 1));
			pclose(foo);
			if(thewindow==0)
			{
				puts("Error!  Can't find window to send events to!");
				return 1;
			}
		}
		parentwin = RegisterCloseEvent(thedisp, thewindow);
		XFlush(thedisp);
		break;
#endif
#ifdef ENABLE_CONSOLE
    case RAWCONSOLE:
    case TERMINAL:
		if(argv[1][0]=='/')
		{
			strcpy(string, argv[1]);
		} else {
			sprintf(string, "/dev/tty%s", argv[1][0] == '-' ? "" : argv[1]);
		}
		if((consolefd=open(string, O_RDONLY, 0)) < 0)
		{
			printf("Can't open %s\n", string);
			return 1;
		}
		break;
#endif
    default:
		puts("Must specify a target!");
		exit(1);
    }

    for(i=0; i<numaxes; i++)
    {
		if(! axis_threshold_defined[i])
			calibrate(i);
    }

    memset(&js, 0, sizeof(struct js_event));

    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);
    if(repeat_time.it_interval.tv_usec)
    {
		signal(SIGALRM, repeat_handler);
		repeat_time.it_value.tv_usec=repeat_time.it_interval.tv_usec;
		setitimer(ITIMER_REAL, &repeat_time, NULL);
    }

    puts("Initialization complete, entering main loop, ^C to exit...");

    /* Main Loop */
    for(;;)
    {
		memset(&js, 0, sizeof(struct js_event));
#ifdef ENABLE_X
		do
		{
/*
  FD_ZERO(&close_set);
  FD_SET(ConnectionNumber(thedisp), &close_set);
  FD_SET(jsfd, &close_set);
  XFlush(thedisp);
  select((ConnectionNumber(thedisp) > jsfd ? ConnectionNumber(thedisp) : jsfd) + 1, 
  &close_set, NULL, NULL, NULL);
  if(FD_ISSET(ConnectionNumber(thedisp), &close_set) && target==X) 
  if(target==X) CheckIfWindowClosed(thedisp, parentwin, thewindow);
*/
			FD_ZERO(&close_set);
			FD_SET(jsfd, &close_set);
			tv.tv_sec=0;
			tv.tv_usec=200000;
			select(jsfd + 1, &close_set, NULL, NULL, &tv);
			if(target==X) CheckIfWindowClosed(thedisp, parentwin, thewindow);
		} while(!FD_ISSET(jsfd, &close_set));

#endif
		read(jsfd, &js, sizeof(struct js_event));


		switch(js.type)
		{
		case JS_EVENT_BUTTON: /* buttons */
			if(button_actions[js.number])
			{
				button_repeat_flags[js.number]=js.value;
				sendkey(button_actions[js.number], 
						js.value ? PRESS : RELEASE, button_upper[js.number]);
			}
			break;
		case JS_EVENT_AXIS: /* axis */
			if(axis_actions[js.number][0] || axis_actions[js.number][1])
			{
				if(axis_hold_flags[js.number])
				{
					switch(axis_hold_flags[js.number])
					{
					case 1:
						if(js.value>axis_threshold[js.number][0])
							sendkey(axis_actions[js.number][0], RELEASE, 0);
						break;
					case 2:
						if(js.value<axis_threshold[js.number][1])
							sendkey(axis_actions[js.number][1], RELEASE, 0);
						break;
					}
					axis_hold_flags[js.number]=0;
				}
				if(js.value<axis_threshold[js.number][0])
				{ 
					if(!axis_actions[js.number][0]) break;
					sendkey(axis_actions[js.number][0], PRESS, 0);
					axis_hold_flags[js.number]=1;
				}
				if(js.value>axis_threshold[js.number][1])
				{
					if(!axis_actions[js.number][1]) break;
					sendkey(axis_actions[js.number][1], PRESS, 0);
					axis_hold_flags[js.number]=2;
				}		
			}	    
			break;
		}
#ifdef ENABLE_X
		if(target==X) XFlush(thedisp);
#endif
    }
}

int check_config(int argc, char **argv)
{
    int i, x, f=0;
    FILE *file;
    int rcargc;
    char *rcargv[255], line[255];
    
    for(i=1; i<argc; i++)
    {
		if(!strcmp("-rcfile", argv[i]))
		{
			if(i+2>argc) 
			{
				puts("Not enough arguments to -rcfile");
				exit(1);
			}
			if((file=fopen(argv[i+1], "r"))==NULL)
			{
				printf("Cannot open rc file \"%s\"\n", argv[i+1]);
				exit(1);
			}
			fscanf(file, " %s ", line);
			while(!feof(file))
			{
				if(!strcmp(line, "COMMON"))
				{
					fscanf(file, " %s ", line);
					for(rcargc=1; strcmp(line, "START") && strcmp(line, "COMMON") 
							&& !feof(file); rcargc++)
					{
						if(line[0]=='#') 
						{
							while(fgetc(file)!='\n');
							rcargc--;
						}
						else rcargv[rcargc]=strdup(line);
						fscanf(file, " %s ", line);
					}
					if(feof(file)) {
						rcargv[rcargc]=strdup(line);
						rcargc++;
					}
					process_args(rcargc, rcargv);
					for(rcargc--;rcargc>0;rcargc--) free(rcargv[rcargc]);
				}		
				fscanf(file, " %s ", line);
			}
			fclose(file);
			common_read=1;
			rcfile=argv[i+1];
			argc-=2;
			for(x=i; x<argc; x++) argv[x]=argv[x+2];
		}
		if(!strcmp("-config", argv[i]))
		{
			if(i+2>argc) 
			{
				puts("Not enough arguments to -config");
				exit(1);
			}
			if(strcmp(rcfile,DEFAULT_RCFILE) == 0)
			{
				x=strlen(getenv("HOME")) + strlen(rcfile) + 2;
				rcfile=(char*)malloc(x);
				sprintf(rcfile, "%s/%s", getenv("HOME"), DEFAULT_RCFILE);
			}
			if((file=fopen(rcfile, "r"))==NULL)
			{
				printf("Cannot open rc file \"%s\"\n", rcfile);
				exit(1);
			}
			if(!common_read)
			{
				fscanf(file, " %s ", line);
				while(!feof(file))
				{
					if(!strcmp(line, "COMMON"))
					{
						fscanf(file, " %s ", line);
						for(rcargc=1; strcmp(line, "START") && strcmp(line, "COMMON") 
								&& !feof(file); rcargc++)
						{
							if(line[0]=='#') 
							{
								while(fgetc(file)!='\n');
								rcargc--;
							}
							else rcargv[rcargc]=strdup(line);
							fscanf(file, " %s ", line);
						}
						if(feof(file)) {
							rcargv[rcargc]=strdup(line);
							rcargc++;
						}
/* XXX			process_args(rcargc+1, rcargv); */
						process_args(rcargc, rcargv);
						for(rcargc--;rcargc>0;rcargc--) free(rcargv[rcargc]);
					}		
					fscanf(file, " %s ", line);
				}
				rewind(file);
				common_read=1;
			}	    
			fscanf(file, " %s ", line);
			while(!feof(file))
			{
				if(!strcmp(line, "START"))
				{
					fscanf(file, " %s ", line);
					if(!strcmp(line, argv[i+1]))
					{
						f=1;
						fscanf(file, " %s ", line);
						for(rcargc=1; strcmp(line, "START") && strcmp(line, "COMMON") 
								&& !feof(file); rcargc++)
						{
							if(line[0]=='#') 
							{
								while(fgetc(file)!='\n');
								rcargc--;
							}
							else rcargv[rcargc]=strdup(line);
							fscanf(file, " %s ", line);
						}
						if(feof(file)) {
							rcargv[rcargc]=strdup(line);
							rcargc++;
						}
/* XXX			process_args(rcargc+1, rcargv); */
						process_args(rcargc, rcargv);
						for(rcargc--;rcargc>0;rcargc--) free(rcargv[rcargc]);
					}
				}	
				fscanf(file, " %s ", line);
			}
			fclose(file);
			if(!f) 
			{
				printf("Can't find config \"%s\"\n", argv[i+1]);
				exit(1);
			}
			argc-=2;
			for(x=i; x<argc; x++) argv[x]=argv[x+2];
		}
    }
    return argc;
}

void process_args(int argc, char **argv)
{
    int i;

    if(!argv[1]) return;
    for(i=(argc == 1 || argv[1][0] == '-') ? 1 : 2; i<argc; i++)
    {
		if(!strcmp(argv[i], "-axis"))
		{
			if(target==NONE) 
			{
				puts("Must specify a target first!");
				exit(1);
			}
			if(i+1==argc) 
			{
				puts("Not enough arguments to -axis");
				exit(1);
			}
			axis_act_counter=0;
			while((i+1)<argc && argv[i+1][0]!='-')
			{		
				if(i+2==argc) 
				{
					puts("Not enough arguments to -axis");
					exit(1);
				}
				i++;
				axis_actions[axis_act_counter][0]=argtokey(argv[i]);
				axis_actions[axis_act_counter][1]=argtokey(argv[++i]);
				axis_act_counter++;
			}
			continue;
		}
		if(!strcmp(argv[i], "-buttons"))
		{
			if(target==NONE) 
			{
				puts("Must specify a target first!");
				exit(1);
			}

			if(i+1==argc) 
			{
				puts("Not enough arguments to -buttons");
				exit(1);
			}
			button_act_counter=0;
			while((i+1)<=argc && (argv[i+1][0]!='-' || 
								 (argv[i+1][0]=='-' && !argv[i+1][1])))
			{
				button_actions[button_act_counter]=argtokey(argv[++i]);
				printf("arg2key fed with %s ",argv[i]);
				/* capture if key is an upcase chracter, so we can set */
				/* shiftmask proper */
				button_upper[button_act_counter]=0;
				if((strlen(argv[i])==1) && (isupper(argv[i][0])))
				{
					button_upper[button_act_counter]=1; 
					printf("%s is a upper => %d\n",argv[i],button_upper[button_act_counter]);
				}
				else
				{
					printf("%s is a lower => %d\n",argv[i],button_upper[button_act_counter]);
				}
				button_act_counter++;
			}
			continue;
		}
		if(!strcmp(argv[i], "-thresh"))
		{
			if(i+1==argc) 
			{
				puts("Not enough arguments to -thresh");
				exit(1);
			}
			while((i+1)<argc && 
				  (argv[i+1][0]!='-' || 
				   (argv[i+1][0]=='-' && isdigit(argv[i+1][1]))))
			{
				i++;
				if(i+1==argc) 
				{
					puts("Not enough arguments to -thresh");
					exit(1);
				}
                if(argv[i][0] != 'x'  && argv[i+1][1] != 'x') 
                    axis_threshold_defined[thresh_counter] = 1;
				axis_threshold[thresh_counter][0]=atoi(argv[i]);
				axis_threshold[thresh_counter++][1]=atoi(argv[++i]);
			}
			continue;
		}
		if(!strcmp(argv[i], "-autorepeat"))
		{
			if(i+1==argc || (argv[i+1][0]=='-')) 
			{
				repeat_time.it_interval.tv_usec=1000000/DEFAULT_AUTOREPEAT;
				continue;
			}
			if((repeat_time.it_interval.tv_usec=atoi(argv[++i]))==0) 
			{ 
				puts("Invalid autorepeat frequency, using default");
				repeat_time.it_interval.tv_usec=1000000/DEFAULT_AUTOREPEAT;
			}
			if(repeat_time.it_interval.tv_usec>50) 
			{
				repeat_time.it_interval.tv_usec=1000000/50;
				printf("Set to maximum 50 times / second.\n");
			}
			repeat_time.it_interval.tv_usec=
				1000000/repeat_time.it_interval.tv_usec;
			continue;
		}
		if(!strcmp(argv[i], "-deadzone"))
		{
			if(i+1==argc || (argv[i+1][0]=='-')) 
			{
				puts("Not enough arguments to -deadzone");
				exit(1);		
				continue;
			}
			if((deadzone=atoi(argv[++i]))==0) 
			{ 
				puts("Invalid deadzone setting, using default");
				deadzone=DEFAULT_DEADZONE;
			}
			if(deadzone>99) 
			{
				deadzone=99;
				puts("Set to maximum 99%");
			}
			if(deadzone<1) 
			{
				deadzone=1;
				puts("Set to minimum 1%");
			}
			continue;
		}

		if(!strcmp(argv[i], "-dev"))
		{
			if(i+1==argc) 
			{
				puts("Not enough arguments to -dev");
				exit(1);
			}
			device=strdup(argv[++i]);
			continue;
		}

#ifdef ENABLE_CONSOLE
		if(!strcmp(argv[i], "-rawconsole"))
		{
			target=RAWCONSOLE;
			continue;
		}
		if(!strcmp(argv[i], "-terminal"))
		{
			target=TERMINAL;
			continue;
		}
#endif
#ifdef ENABLE_X
		if(!strcmp(argv[i], "-X"))
		{
			target=X;
			thedisp=XOpenDisplay(NULL);
			if(thedisp==NULL) 
			{
				puts("Error opening X Display");
				exit(1);
			}
			continue;
		}
#endif
		printf("Unknown option %s\n", argv[i]);
		puts("Usage: joy2key [\"Window Name\"]");
#ifdef ENABLE_CONSOLE
		printf("       [ -rawconsole ]");
		printf("\n       [ -terminal ]");
#endif
#ifdef ENABLE_X
		printf("\n       [ -X ]");
#endif
		printf("\n       [ -axis [(axis0) low hi] [(axis1) low hi]  ... ]");
		printf("\n       [ -thresh [(axis0) low hi] [(axis1) low hi]  ... ]");
		printf("\n       [ -buttons [(button0)] [(button1)] [(button2)] ... ]");
		printf("\n       [ -dev {%s} ]", DEFAULT_DEVICE);
		printf("\n       [ -rcfile {%s} ]", DEFAULT_RCFILE);
		printf("\n       [ -config {no default} ]");
		printf("\n       [ -autorepeat {(freq) %i} ]", DEFAULT_AUTOREPEAT);
		printf("\n       [ -deadzone {(percent) %i} ]", DEFAULT_DEADZONE);

		puts("\n\nnote: [] denotes `optional' option or argument,");
		puts("      () hints at the wanted arguments for options");
		puts("      {} denotes default (compiled-in) parameters");
		exit(1);
    }

	
}

void calibrate(int num)
{
    struct js_event js;
    int joymid=0;

    printf("\nCalibrating axis %i\n", num);
    printf("Please center the axis and press a button.\n?");
    fflush(stdout);
    do
    {
		read(jsfd, &js, sizeof(struct js_event));
		if(js.type==JS_EVENT_AXIS && js.number==num) 
		{
			joymid=js.value;
			printf("\rvalue: %i   ", joymid);
			fflush(stdout);
		}	
    } while(js.type!=JS_EVENT_BUTTON || (js.type==JS_EVENT_BUTTON && js.value==0));
    printf("\nLocked at %i", joymid);
    printf("\nPlease move the axis to its lowest position and press a button.\n?");
    fflush(stdout);
    do
    {
		read(jsfd, &js, sizeof(struct js_event));
		if(js.type==JS_EVENT_AXIS && js.number==num) 
		{
			axis_threshold[num][0]=js.value;
			printf("\rvalue: %i   ", axis_threshold[num][0]);    
			fflush(stdout);
		}
    } while(js.type!=JS_EVENT_BUTTON || (js.type==JS_EVENT_BUTTON && js.value==0));
    printf("\nPlease move the axis to its highest position and press a button.\n?");
    fflush(stdout);
    do
    {
		read(jsfd, &js, sizeof(struct js_event));
		if(js.type==JS_EVENT_AXIS && js.number==num) 
		{
			axis_threshold[num][1]=js.value;
			printf("\rvalue: %i   ", axis_threshold[num][1]);
			fflush(stdout);
		}
    } while(js.type!=JS_EVENT_BUTTON || (js.type==JS_EVENT_BUTTON && js.value==0));
    printf("\nUsing deadzone of %i%%\n", deadzone);
    axis_threshold[num][0]=joymid+((axis_threshold[num][0] - joymid) * (deadzone/100.0));
    axis_threshold[num][1]=joymid+((axis_threshold[num][1] - joymid) * (deadzone/100.0));
    axis_threshold_defined[num] = 1;
    puts("Calibrations set at:");
    printf("Axis %i low threshold set at %i\n", num, axis_threshold[num][0]);
    printf("Axis %i high threshold set at %i\n", num, axis_threshold[num][1]);    
    puts("(you can put these in your .joy2keyrc to avoid calibrating in the future)");
}

int argtokey(char *arg)
{
    int ret;
    switch(target)
    {
#ifdef ENABLE_X
    case X:
		if((ret=XStringToKeysym(arg))==NoSymbol) 
			printf("argtokey:Can't find %s, check include/X11/keysymdef.h\n", arg);
		printf("argtokey:read key %s dblcheck %s\n",arg,  XKeysymToString(XStringToKeysym(arg)) );
		ret=XKeysymToKeycode(thedisp,XStringToKeysym(arg));
		return ret;
#endif
#ifdef ENABLE_CONSOLE
    case RAWCONSOLE:
    case TERMINAL:
		if(arg[1])
		{
			sscanf(arg, "%i", &ret);
		}
		else
		{
			ret=arg[0];
		}
		return ret;
#endif
    }
    return 0;
}

void repeat_handler(int s)
{
    int i;

    signal(SIGALRM, SIG_IGN);

    for(i=0; i<button_act_counter; i++)
    {
		if(button_repeat_flags[i])
		{
			sendkey(button_actions[i], RELEASE, button_upper[i]);
			sendkey(button_actions[i], PRESS, button_upper[i]);
		}
    }
#ifdef ENABLE_X
    if(target==X) XFlush(thedisp);
#endif
    signal(SIGALRM, repeat_handler);
}

void sendkey(unsigned int keycode, press_or_release_type PoR, int iscap)
{
#ifdef ENABLE_X
    static XEvent event;
    static char needinitxev=1;
#endif
    char conkey;
printf("iscap is now %d  ",iscap);
    switch(target)
    {
#ifdef ENABLE_X
    case X:
		if(needinitxev)
		{
			memset(&event, 0, sizeof(event));
			event.xkey.type=KeyPress;
			event.xkey.root=RootWindow(thedisp, thescreen);
			event.xkey.window=thewindow;
			event.xkey.subwindow=thewindow;
			event.xkey.same_screen=True;
			event.xkey.time=CurrentTime;
			event.xkey.display=thedisp;
			needinitxev=0;
		}
		event.xkey.type=PoR==PRESS ? KeyPress : KeyRelease;
		event.xkey.state=0x10;
		event.xkey.keycode=keycode;
		printf("sendkey: button_upper: %d keycode  0x%06x  %0d   ",iscap,event.xkey.keycode,keycode  );
		printf("sendkey: keysym %s\n",XKeysymToString(XKeycodeToKeysym(thedisp, event.xkey.keycode,0) ));
		printf("state hex %x\n",event.xkey.state);
		/* set proper shiftmask */
		/* check if numpad */
		if((event.xkey.keycode>=79) && (event.xkey.keycode<=90))
		{
			event.xkey.state=0x10;
		}
		else /* not numpad */
		{
			if ( iscap > 0  )
			{
				printf("executing iscap > 0\n");
				switch ( PoR )
				{
				case PRESS : 
					printf("executing iscap > 0 , sending shiftkey first.\n");
					/* send shift */
					event.xkey.keycode=XKeysymToKeycode(thedisp,XK_Shift_L);
					event.xkey.type= KeyPress ;
					event.xkey.state=0x10 ;
					XSendEvent(thedisp, thewindow, False, event.xkey.state, &event);
					sleep(0.1);
					/* send key */
					event.xkey.keycode=keycode;
					event.xkey.state=0x11;  /* set shift aan */
					XSendEvent(thedisp, thewindow, False, event.xkey.state, &event);
					break;

				case RELEASE:
					printf("iscap > 0: RELEASE , sending shiftkey last.\n");
					/* release key */
					event.xkey.type= KeyRelease ;
					event.xkey.state=0x11;  /* set shift aan */
					XSendEvent(thedisp, thewindow, False, event.xkey.state, &event);
					/* release shiftkey */
					sleep(0.2);
					event.xkey.keycode=XKeysymToKeycode(thedisp,XK_Shift_L);
					XSendEvent(thedisp, thewindow, False, event.xkey.state, &event);
					break;
				default:
					printf(" not understood PoR\n");
				break;
				} /* END OF PoR  SWITCH */
			} /* end isupper iscap */
		} /* else if numpad => ie not numpad, check for uppercase */
		/* check for num keypad. "+", "-" and enter do not change meaning, so no problem */
		/* setting blunt ox10 state */
		
		XSendEvent(thedisp, thewindow, False, event.xkey.state, &event);
		sleep(0.2);

		break;

#endif
#ifdef ENABLE_CONSOLE
    case RAWCONSOLE:
		if(PoR==PRESS) 
			conkey=keycode;
		if(PoR==RELEASE)
			conkey=keycode | 0x80;
		ioctl(consolefd, TIOCSTI, &conkey);
		break;
    case TERMINAL:
		if(PoR==PRESS)
		{
			conkey=keycode;
			ioctl(consolefd, TIOCSTI, &conkey);
		}
#endif
    }
}

void cleanup(int s)
{
    printf("\n%s caught, cleaning up & quitting.\n", 
		   s==SIGINT ? "SIGINT" : 
		   (s==SIGTERM ? "SIGTERM" : ((s == 0) ? "Window die" : "Unknown")));
/* Because the window has just closed, it will print out an error upon
   calling these functions.  To suppress this superflous error, don't call 
   them :) */
/*    XFlush(thedisp); */
/*    XCloseDisplay(thedisp); */
#ifdef ENABLE_CONSOLE
    if(target==RAWCONSOLE || target==TERMINAL) close(consolefd);
#endif
    exit(0);
}

#ifdef ENABLE_X
static Window
RegisterCloseEvent(Display	*disp,
				   Window	win)
{
    Window		root_return;
    Window		parent_return = 0;
    Window		*children_return;
    unsigned int	nchildren_return;

    XQueryTree(disp, win, &root_return, &parent_return, &children_return,
			   &nchildren_return);
    if (parent_return) {
		XSelectInput(disp, parent_return, SubstructureNotifyMask);
    }
    if (nchildren_return > 0) {
		XFree(children_return);
    }
    return parent_return;
}

static void
CheckIfWindowClosed(Display	*disp,
					Window	parentwin,
					Window	win)
{
    XEvent	event;


    if (XCheckWindowEvent(disp, parentwin, SubstructureNotifyMask, &event) 
		== True) {
		if (event.type == DestroyNotify && event.xdestroywindow.window 
			== win) {
			cleanup(0);
		}
    }
}
#endif
