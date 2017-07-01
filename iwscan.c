/* Based on
 *	Wireless Tools
 * by
 *		Jean II - HPLB '99 - HPL 99->07
 *
 * This tool can access various piece of information on the card
 * not part of iwconfig...
 *
 * This file is released under the GPL license.
 *     Copyright (c) 1997-2007 Jean Tourrilhes <jt@hpl.hp.com>
 *     Disfigured in 2016 by Sergey Ptashnick <0comffdiz@inbox.ru>
 */

#include "iwscan.h"

#define MYNAME "iwscan"

static void iwscan_usage(int status)
{
  FILE *		fd = status ? stderr : stdout;

  fprintf(fd, "Usage: " MYNAME " [<INTERFACE>|all [essid <ESSID>] [last] [to <FILE>]]\n");
  fprintf(fd, "Note that \"essid\" parameter may not work (depend on your card' driver)\n");
  exit(status);
}

static void iwscan_version(void)
{	
  fprintf(stdout, MYNAME " version " VERSION "\n");
  exit(0);
}


int
main(int	argc,
     char **	argv)
{
  int skfd;			/* generic raw socket desc.	*/
  char *dev;			/* device name			*/
  char **args;			/* Command arguments */
  char *fname = NULL;		/* file name */
  int fnlen;			/* file name length */
  char **argt;			/* temporary pointer for fname search */
  int count, cnt;		/* Number of arguments */
  int goterr = 0;
  FILE * fd = stdout;           /* fd for output */

  /* Those don't apply to all interfaces */
  if((argc == 2) && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")))
    iwscan_usage(0);
  if((argc == 2) && (!strcmp(argv[1], "-v") || !strcmp(argv[1], "--version")))
    iwscan_version();

  if(argc == 1)
    {
      /*dev = "wlan0"; */
      /*dev = NULL; */
      dev = IWSCAN_ALL_IFACES;
      args = NULL;
      count = 0;
    }
  else
    {
      dev = argv[1];
      args = argv + 2;
      count = argc - 2;
      /*if (!strcmp(dev, IWSCAN_ALL_IFACES)) 
        {
          dev = NULL;
        }
      */

      argt = args;
      cnt = count;
      while (cnt > 0)
        {
	  cnt--;
	  if (!strcmp(argt[0], "to"))
	    {
              if (cnt < 1)
                {
		  fprintf(stderr, MYNAME 
                      ": too few arguments for scanning option [%s]\n",
		      argt[0]);
		  iwscan_usage(-1);
                }
              argt++;
	      cnt--;
	      fnlen = strlen(argt[0]);
	      fname = malloc(fnlen+1);
	      strcpy(fname, argt[0]);
	    }
          argt++;
	}
    }

  /* Open dump file */
  if (fname)
    {
      fd = fopen(fname, "w");
      if (!fd)
        {
	  perror(MYNAME ": cannot open output file");
	  return(-1);
	}
    }

  /* Create a channel to the NET kernel. */
  if((skfd = iw_sockets_open()) < 0)
    {
      perror(MYNAME ": cannot open IW socket");
      return -1;
    }

  fprintf(fd, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
  /* do the actual work */
  if (dev)
    goterr = iws_print_scanning_info_xml(fd, skfd, dev, args, count);

  /* Close the socket. */
  iw_sockets_close(skfd);

  if (fname)
    {
      fclose(fd);
      free(fname);
    }

  return goterr;
}

