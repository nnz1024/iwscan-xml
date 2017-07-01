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


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <time.h>

#include "iwscan.h"

#define DEFAULT_PID_FILE ( IWSCAND_PID_FILE_DIR "/iwscand.pid" )

#define MYNAME "iwscand"
#define DEVNULL "/dev/null"

#define PAR_PIDFILE 0x1
#define PAR_NONBLOCK 0x2
#define PAR_SAFE 0x4

#define STATE_FREE 0x0
#define STATE_WORK 0x1
#define STATE_STOP 0x2

int global_state = 0;

static void iwscand_usage(int status)
{
  FILE *		fd = status ? stderr : stdout;

  fprintf(fd, "\nUsage: " MYNAME " <INTERFACE> to <FILE> [safe|nonblock] "
      "[essid <ESSID>] [pid [<PIDFILE>]]\n\n");
  fprintf(fd, "Note that \"essid\" parameter may not work (depends on your card' driver).\n");
  fprintf(fd, "If you specify \"pid\" as last parameter, <PIDFILE> will be \"%s\".\n\n",
      DEFAULT_PID_FILE);
  exit(status);
}

static void iwscand_version(void)
{	
  fprintf(stdout, "\n" MYNAME " version " VERSION "\n\n");
  exit(0);
}

void iwscand_works(int signal)
{
  switch (signal)
    {
      case SIGUSR1:
        global_state |= STATE_WORK;
        break;
      case SIGTERM:
        global_state |= STATE_STOP;
        break;
    }
}

int
main(int	argc,
     char **	argv)
{
  int skfd;			/* Generic raw socket descripto r*/
  char *dev;			/* Device name */
  char **args;			/* Command arguments */
  int params = 0;               /* Parameter flags */
  char *out_fname = NULL;       /* Output file name */
  int fnlen;                    /* File name length */
  char *pid_fname = NULL;       /* Name of PID file */
  char *pid_fname_real = NULL;  /* Real name of PID file - this allow to use 
                                 *  . and .. in pid_fname and unlink
                                 *  such files after chdir */
  char pid_fname_default[] = DEFAULT_PID_FILE; /* Source for strcpy - */
                                /* this allows us to free(pid_fname) */
  char **argt;			/* temporary pointer for fname search */
  int count, cnt;		/* Number of arguments */
  FILE * fd, * pd;              /* fds for output file and for pid file */
  int goterr = 0;               /* Error indicator */
  pid_t pid;                    /* For checks after forking */
  struct timespec cycle_wait = {.tv_sec = 0, .tv_nsec = 10000000 }; /* 10 ms */
  char stamp_buf[IWS_TIMESTAMP_LENGTH];

  /* Those don't apply to all interfaces */
  if((argc == 2) && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")))
    iwscand_usage(0);
  if((argc == 2) && (!strcmp(argv[1], "-v") || !strcmp(argv[1], "--version")))
    iwscand_version();

  if(argc < 4)
    {
      iwscand_usage(-1);
    }
  else
    {
      dev = argv[1];
      args = argv + 2;
      count = argc - 2;

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
		  iwscand_usage(-1);
                }
              argt++;
	      cnt--;
	      fnlen = strlen(argt[0]);
	      out_fname = malloc(fnlen+1);
	      strcpy(out_fname, argt[0]);
	    }
          else if (!strcmp(argt[0], "pid"))
            {
              if (cnt < 1)
                {
                  fnlen = strlen(pid_fname_default);
                  pid_fname = malloc(fnlen+1);
                  strcpy(pid_fname, pid_fname_default);
                }
              else
                {
                  argt++;
                  cnt--;
                  fnlen = strlen(argt[0]);
                  pid_fname = malloc(fnlen+1);
                  strcpy(pid_fname, argt[0]);
                }
              params |= PAR_PIDFILE;
            }
          else if (!strcmp(argt[0], "safe"))
            {
              if (params & PAR_NONBLOCK) 
                {
                  fprintf(stderr, MYNAME 
                      ": paramaters \"safe\" and \"nonblock\" are mutually "
                      "exclusive.\n");
                  iwscand_usage(-1);
                }
              params |= PAR_SAFE;
            }
          else if (!strcmp(argt[0], "nonblock"))
            {
              if (params & PAR_SAFE) 
                {
                  fprintf(stderr, MYNAME 
                      ": paramaters \"safe\" and \"nonblock\" are mutually "
                      "exclusive.\n");
                  iwscand_usage(-1);
                }
              params |= PAR_NONBLOCK;
            }
          argt++;
	}
    }

  /* Protecting from some strange people */
  if (!strcmp(dev, IWSCAN_ALL_IFACES))
    {
      fprintf(stderr, MYNAME ": scanning all interfaces are not allowed by "
          MYNAME ". Try to use iwscan.\n");
      iwscand_usage(-1);
    }

  /* Open output file */
  if (out_fname)
    {
      if (params & PAR_SAFE)
        unlink(out_fname);
      if (params & PAR_NONBLOCK) 
        {
          /* Trying to open file (FIFO?) in non-blocking mode */
          skfd = open(out_fname, O_CREAT|O_TRUNC|O_WRONLY|O_NONBLOCK);
          goterr = (skfd < 0);
          if (!goterr)
            {
              fd = fdopen(skfd, "w");
              goterr = (fd == NULL);
              skfd = -1;
            }
        }
      else
        {
          fd = fopen(out_fname, "w");
          goterr = (fd == NULL);
        }
      if (goterr)
        {
	  perror(MYNAME ": cannot open output file");
	  return -1;
	}
      free(out_fname);
    }
  else
    {
      fprintf(stderr, MYNAME ": output file (\"to FILE\") is mandatory commandline "
          "parameter!\n");
      iwscand_usage(-1);
    }

  if (params & PAR_PIDFILE)
    {
      unlink(pid_fname); /* Uhmmm, TOCTOU, again? */
      pd = fopen(pid_fname, "w");
      if (!pd)
        {
          fclose(fd);
          perror(MYNAME ": cannot open PID file");
          return -1;
        }
      /* I hope OS will allow us to resolve name at this stage... */
      pid_fname_real = realpath(pid_fname, NULL);
      free(pid_fname);
      if (!pid_fname_real)
        {
          fclose(fd);
          fclose(pd);
          perror(MYNAME ": cannot resolve PID file name");
          return -1;
        }
    }

  /* Create a channel to the NET kernel. */
  skfd = iw_sockets_open();
  if(skfd < 0)
    {
      perror(MYNAME ": cannot open IW socket");
      goterr = 1;
      goto cleanup;
    }

  /* Daemonize */

  /* First fork */
  pid = fork();
  if (pid < 0)
    {
      goterr = 1;
      goto cleanup;
    }
  if (pid > 0)
    return 0;

  /* Set SID */
  if (setsid() < 0)
    {
      goterr = 1;
      goto cleanup;
    }

  /* Set signal handlers */
  signal(SIGUSR1, iwscand_works);
  signal(SIGTERM, iwscand_works);

  /* Second fork */
  pid = fork();
  if (pid < 0)
    {
      goterr = 1;
      goto cleanup;
    }
  if (pid > 0)
    return 0;

  /* Setting umask and dir */
  umask(0);
  if (chdir("/") < 0)
    {
      goterr = 1;
      goto cleanup;
    }

  /* "Close" 0, 1 and 2 without UB */
  /* print_scanning_info_xml may write to stderr */
  freopen(DEVNULL, "r", stdin);
  freopen(DEVNULL, "w", stdout);
  freopen(DEVNULL, "w", stderr);

  /* Dump our PID to pidfile and close it */
  if (params & PAR_PIDFILE)
    {
      pid = getpid();
      fprintf(pd, "%ld\n", (long) pid);
      fclose(pd);
    }

  /* Set timestamp */
  iws_format_timestamp((char*) &stamp_buf);

  /* We're ready. Let's tell about that! */
  fprintf(fd, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
  fprintf(fd, "<Scan stamp='%s'>\n", (char *) &stamp_buf);
  fflush(fd);

  /* do the actual work */
  while (!(global_state & STATE_STOP))
    {
      if (global_state & STATE_WORK)
        {
          global_state = STATE_FREE;
          goterr = iws_print_scanning_info_xml(fd, skfd, dev, args, count);
          fprintf(fd, "\n");
          fflush(fd);
        }
        nanosleep(&cycle_wait, NULL);
    }

  fprintf(fd, "</Scan>\n");
  goterr = 0;

cleanup:
  /* Close the socket */
  if (!(skfd < 0)) /* If there is no error, of course */
    iw_sockets_close(skfd);

  /* Close output file */
  if (fd)
    fclose(fd);

  /* Remove PID file */
  if (params | PAR_PIDFILE)
    {
      unlink(pid_fname_real);
      free(pid_fname_real);
    }

  return -goterr;
}

