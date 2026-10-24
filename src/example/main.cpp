//---------------------------------------------------------------------------
//
// MODULE NAME:     main.cpp
//
// COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
//                  file within the Seminole distribution for terms of use.
//                  If your distribution is incomplete, the Seminole
//                  documentation and license are available on the World
//                  Wide Web at http://www.conman.org/software/seminole/
//
// DESCRIPTION:     Example program using the Seminole embedded web server.
//
// AUTHOR:          M. Grosberg
//
// CREATION DATE:   Oct 28, 2001
//
//---------------------------------------------------------------------------
//
// HISTORY OF CHANGE  
//
// -----+--------+------------------------------------------+------+---------
// VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
// =====+========+==========================================+======+=========
//  001 |10/28/01| File creation.                           | MYG  |  
// -----+--------+------------------------------------------+------+---------
//  002 |01/11/02| Initialize TCP/IP if on Win32.           | MYG  |
// -----+--------+------------------------------------------+------+---------
//  003 |06/24/02| Add default root if none is specified.   | MYG  |
// -----+--------+------------------------------------------+------+---------
//---------------------------------------------------------------------------

#include "seminole.h"

// Some systems don't seem to define these:
#ifndef EXIT_SUCCESS
# define EXIT_SUCCESS 0
#endif
#ifndef EXIT_FAILURE
# define EXIT_FAILURE 1
#endif

static Httpd *webServer;
static bool   haveRoot = false;

//---------------------------------------------------------------------------
//
// NAME:           usage
//
// DESCRIPTION:    Print a command line summary. 
//
// RETURNS:        An exit code to the calling program or OS.
//
// REMARKS:        The simple command line format is described here.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jul 15, 2001
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
static int usage(const char **argv)
{
  printf("Usage:\n\t%s host:port <prefix>=type:<path> ...\n",
         argv[0]);
 
  return (EXIT_FAILURE);
}

//---------------------------------------------------------------------------
//
// NAME:           parse_arguments
//
// DESCRIPTION:    Parse the command line and build the path list
//                 and get the server id.
//
// REMARKS:        This should really parse a configuration file but 
//                 that's just not a priority right now. 
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jul 15, 2001
// LAST MODIFIED:  Jan 25, 2002  BY:  MYG (Handle pkg filesystem)
//
//--------------------------------------------------------------------------
static void parse_arguments(int argc, const char **argv)
{
  HttpdIpPort port;

  ///////////////////////////////////////////////////////////////////
  // Get server identification. 
  ///////////////////////////////////////////////////////////////////

  char *p_host = HttpdUtilities::StrVCat(argv[1], (char *)0);
  if (p_host == NULL) return;

  char *p_colon = strchr(p_host, ':');
  if (p_colon == NULL)
    port = 80;
  else
  {
    *p_colon++ = '\0';
    port = strtoul(p_colon, NULL, 0);
  }

  webServer = new Httpd(port, p_host);
  HttpdOpSys::Free(p_host);
  if (webServer == NULL) return;  

  HttpdFileSystem *p_native_fs = HttpdOpSys::NativeFileSystem();

  ///////////////////////////////////////////////////////////////////
  // Parse exported directories. 
  ///////////////////////////////////////////////////////////////////

  for(int i = 2; i < argc; i++)
  {
    char *p_save = HttpdUtilities::StrVCat(argv[i], (char *)0);
    if (p_save == NULL) continue;

    char *p_sep = strchr(p_save, '=');
    if (p_sep != NULL)
    {
       *p_sep++ = '\0';

       char *p_colon = strchr(p_sep, ':');
       if (p_colon == NULL)
       {
         printf("No encapsulation type found in %s\n", p_sep);
         HttpdOpSys::Free(p_save);
         continue;
       }

       *p_colon++ = '\0';

       HttpdFileSystem *p_fs;

       if (strcmp(p_sep, "sys") == 0)
       {
          if (p_native_fs == NULL)
          {
            printf("Sorry, no native file system exists for %s\n", p_colon);
            HttpdOpSys::Free(p_save);
            continue;
          }

          p_fs   = p_native_fs;
       }
       else if (strcmp(p_sep, "pkg") == 0)
       {
#ifdef HTTPD_HAVE_NATIVE_FILE_SOURCES
          HttpdDataSource *p_source;

          if (HttpdOpSys::OpenSystemFile(p_colon, p_source) != 0)
          {
             printf("Can't access file %s\n", p_colon);
             HttpdOpSys::Free(p_save);
             continue;
          }

          HttpdRomFileSystem *p_rom_fs = new HttpdRomFileSystem;

          if (p_rom_fs->Mount(p_source) != 0)
          {
             printf("File %s does not contain a valid package\n", p_colon);
             delete p_rom_fs;
             HttpdOpSys::Free(p_save);
             HttpdOpSys::CloseSystemFile(p_source);
             continue;
          }

          p_fs    = p_rom_fs;
          p_colon = "/";
#else
          printf("Sorry, packages are not supported in this build.\n");
          exit(1);
#endif
       }
       else
       {
          printf("File specifier %s not recognized\n", p_sep);
          HttpdOpSys::Free(p_save);
          continue;
       }

       HttpdFileHandler *p_hand;

       if (strcmp(p_save, "/") == 0)
         haveRoot = true;

       p_hand = new HttpdFileHandler(p_fs,
                                     p_colon, 
                                     p_save);
       if (p_hand != NULL)
         webServer->Install(p_hand);
    }

    HttpdOpSys::Free(p_save);
  }
}

//---------------------------------------------------------------------------
//
// NAME:           add_default_root
//
// DESCRIPTION:    Add a default root handler from the provided example
//                 content.
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jun 24, 2002
// LAST MODIFIED:  N/A           BY:  N/A
//
//--------------------------------------------------------------------------
static void add_default_root()
{
#ifdef HTTPD_HAVE_NATIVE_FILE_SOURCES
  HttpdDataSource *p_source;

  if (HttpdOpSys::OpenSystemFile("semweb.pkg", p_source) != 0)
  {
    printf("Can't find default content semweb.pkg\n");
    return;
  }

  HttpdRomFileSystem *p_rom_fs = new HttpdRomFileSystem;

  if (p_rom_fs->Mount(p_source) != 0)
  {
    printf("File semweb.pkg is not valid.\n");
    delete p_rom_fs;
    HttpdOpSys::CloseSystemFile(p_source);
    return;
  }

  HttpdHandler *p_hand = new HttpdFileHandler(p_rom_fs,
                                              "/", 
                                              "/");
  if (p_hand != NULL)
    webServer->Install(p_hand);
#endif
}

//---------------------------------------------------------------------------
//
// NAME:           main
//
// DESCRIPTION:    Entry point.
//
// RETURNS:        An exit code to the calling program or OS.
//
// REMARKS:        Parse the command line and start the webserver and
//                 wait for a command (currently only termination).
//
// AUTHOR:         M. Grosberg   
// CREATED:        Jul 15, 2001
// LAST MODIFIED:  Jan 11, 2002  BY:  MYG (Add Win32 startup logic)
//                 Jun 24, 2002       MYG (Add default root if needed)
//
//--------------------------------------------------------------------------
int main(int argc, const char **argv)
{
  if (argc < 2)
    return usage(argv);
  else
    parse_arguments(argc, argv);

  if (!haveRoot)
    add_default_root();

  // I don't understand why I have to do this here for damn Win32. 
  // How about just letting me start TCP/IP without this non-portable
  // stuff in main().
#ifdef WIN32
  WORD     wsvers = MAKEWORD(2, 0);
  WSADATA  wsadata;

  WSAStartup(wsvers, &wsadata);
#endif

  if (!webServer->Start())
  {
    perror("Couldn't start.\n");
    return (EXIT_FAILURE);
  }

  char buf[4];
  printf("Press <ENTER> to stop.\n");
  (void )fgets(buf, sizeof(buf), stdin);

  webServer->Stop();

  return (EXIT_SUCCESS);
}

