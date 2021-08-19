#import <sys/types.h>
#import <mon/global.h>

char boot_action[32], *boot_act = boot_action, *line = "extended diagnostics";

main()
{
	struct mon_global *mg = (struct mon_global*) restore_mg();
	struct nvram_info *ni = &mg->mg_nvram;

	if (0 /* extended diagnostics failed */) {
	
		/* must call alert to bring ROM window on screen */
		mg->mg_alert ("\nExtended Diagnostics failed\n");
		strcpy (boot_action, "-h");	/* just halt */
	} else
		strcpy (boot_action, ni->ni_bootcmd);
		
	/* return to ROM monitor and (possibly) boot kernel */
	asm ("movl _boot_act, d0");
	asm ("trap #13");
}

