#!/usr/bin/perl -w
##---------------------------------------------------------------------------
##
## MODULE NAME:     makedeps.pl
##
## COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
##                  file within the Seminole distribution for terms of use.
##                  If your distribution is incomplete, the Seminole
##                  documentation and license are available on the World
##                  Wide Web at http://www.conman.org/software/seminole/
##
## DESCRIPTION:     Makefile dependency generator
##
## AUTHOR:          A. Hoerter
##
## CREATION DATE:   Oct 18, 2001
##
##---------------------------------------------------------------------------
##
## HISTORY OF CHANGE
##
## -----+--------+------------------------------------------+------+---------
## VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
## =====+========+==========================================+======+=========
##  001 |10/18/01| File creation.                           | AMH  |
## -----+--------+------------------------------------------+------+---------
##  002 |10/25/01| Perform recursion inside the target dir  | AMH  |
## -----+--------+------------------------------------------+------+---------
##  003 |10/27/01| Compute DEPTH automatically.             | MYG  |
##      |        | Added generic rules factoring.           |      |
## -----+--------+------------------------------------------+------+---------
##  004 |12/25/01| Remove cc-isms from generated Makefiles. | MYG  | M1006
## -----+--------+------------------------------------------+------+---------
##  005 |01/11/02| Use correct path sep on Win32.           | GJK  | 
## -----+--------+------------------------------------------+------+---------
##  006 |01/24/02| Support host-based tools.                | MYG  | 
## -----+--------+------------------------------------------+------+---------
##  007 |03/29/02| Fix problem found on MkLinux/PPC.        | MYG  | M1013
## -----+--------+------------------------------------------+------+---------
##  008 |06/23/02| Add support for headers on TOOLS.        | MYG  | M1021
## -----+--------+------------------------------------------+------+---------
##  009 |06/24/02| Work around NT4 build bugs.              | MYG  |
## -----+--------+------------------------------------------+------+---------
##  010 |06/29/02| Work around Solaris build bugs.          | MYG  | S1005
## -----+--------+------------------------------------------+------+---------
##---------------------------------------------------------------------------

use strict;
use Getopt::Std;
use File::Find;
use File::Spec;

###
#
# Make file generator for Seminole.
#
# The required '-d' option specifies a directory to recurse within,
# while '-c' specifies that output makefiles generated from generic
# files are to be deleted ('clean').
#
###

## globals

# Determine if the O/S we are running on is a broken one. 
my $backwards_path_sep = ($^O eq 'dos' 
                       || $^O eq 'MSWin32' 
                       || $^O eq 'os2'
                       || $^O eq 'cygwin');
my $path_sep           = $backwards_path_sep ? '\\' : '/';

# A real makefile is the end result
my $output_name = "Makefile";

# makefile template
my $mktmpl_name = "Makefile.gen";

# from Getopt::Std
use vars qw($opt_c $opt_d $opt_r);

# Rule library.
my %rules;
my $compile_cmd      = undef;
my $host_compile_cmd = undef;
my $mktool_cmd       = undef;

## Subroutine : read_rulelib
## Purpose    : To keep platform specifics from the makefile templates,
##              this routine reads in the platform 'dependant' bits
##              from a "rulelib" file that can be selected based on 
##              toolchain.
## Inputs     : Filename of rules file..
## Outputs    : None. 
## Assumptions: Fills in the global %rules.
sub read_rulelib
{
  my $fn = shift;
  my $tag;

  open(RL, $fn) or die "Can't open rule file.";
  while (<RL>)
  {
    # I hate Win32 for their silly broken text-file format. 
    # It seems that some ports of Perl (but not all) keep the
    # \r when reading a text file. I am 100% sure this is broken
    # behavior; but either way I just fudge it.
    s/\r$// if $backwards_path_sep;

    if (/^DEFINE\s+(.*)$/)
    { $tag = $1; } 
    elsif (/^COMPILE\s+(.*)$/)
    { $compile_cmd = $1; $tag = ''; }
    elsif (/^HOST_COMPILE\s+(.*)$/)
    { $host_compile_cmd = $1; $tag = ''; }
    elsif (/^PERL_MKTOOL\s+(.*)$/)
    { $mktool_cmd = $1; $tag = ''; }
    elsif ($tag ne '')
    { $rules{$tag} .= $_; }
    elsif (!/^\s+$/)
    { die "Rule line out of scope: $_\n"; }
  }
  close(RL);

  if (!defined $compile_cmd)
  { die "No compilation command defined.\n"; }
}

## Subroutine : print_deps
## Purpose    : Search for .cpp and .h files in the current directory, and 
##              generate appropriate make(1) dependencies and rules.
## Inputs     : Reference to "pubincs" and "xincs" arrays.
## Outputs    : None.
## Assumptions: An appropriate output file handle should be selected unless
##              STDOUT is acceptable.
sub print_deps
{
    my $pubincs = shift;
    my $xincs   = shift;
    my $count;

    # Step 1: Public includes.
    if (@$pubincs)
    {
      print "PUBINCS= \\\n";

      $count = scalar(@$pubincs);
      foreach (@$pubincs)
      { 
        print "   \$(INCOUT)${path_sep}$_" 
              . (($count-- > 1) ? ' \\' : '') . "\n"; 
      }
      print "\nAPIFILES= \\\n";
      
      $count = scalar(@$pubincs);
      foreach (@$pubincs)
      { 
        print "   $_"
            . (($count-- > 1) ? ' \\' : '') . "\n"; 
      }
      print "\n";
    }
 
    # Step 2: Objects.
    my @h_list      = sort glob("*.h");
    my @cpp_list    = sort glob("*.cpp");
 
    return unless (@cpp_list);

    push(@h_list, @$xincs);
    print "OBJS= \\\n";

    $count = scalar(@cpp_list);
    foreach (@cpp_list)
    {
        (my $o_name = $_) =~ s/\.cpp/\.\$(OBJSFX)/;
        print "    \$(BUILT_DIR)${path_sep}lib${path_sep}$o_name"
              . (($count-- > 1) ? ' \\' : '') . "\n";
    }
    print "\n";


    foreach (@cpp_list)
    {
        (my $o_name = $_)           =~ s/\.cpp/\.\$(OBJSFX)/;
        (my $c_cmd  = $compile_cmd) =~ s/__SOURCES__/$_/ge;

        print "\$(BUILT_DIR)${path_sep}lib${path_sep}$o_name: $_ @h_list\n";
        print "\t$c_cmd\n";
    }
}

## Subroutine : compute_depth
## Purpose    : Compute the value of the DEPTH makefile variable. 
##              This variable is used to make all pathnames relative
##              to the root of the source tree.
## Flames:      I originally had this routine written so that it was
##              very portable across different architectures by using
##              File::Spec. It seems that File::Spec only really works
##              on very new Perl distributions. So I'll just re-implement
##              this to be as non-portable as make. 
## Inputs     : the filespec of the directory that will contain the 
##              generated Makefile.
## Outputs    : Value of $(DEPTH)
## Assumptions: 
sub compute_depth
{
    my $d = $File::Find::dir;
    if ($backwards_path_sep)
    { $d =~ s/\//$path_sep/ge; }
    
    my @dirs = split(/\Q$path_sep\E/, $d);
    my $file;

    foreach $file (@dirs)
    { $file = File::Spec->updir; }

    return join($path_sep, @dirs);
}

## Subroutine : print_tooldeps
## Purpose    : Scan for tool source files and build a list
##              of targets and generate the appropriate makeage to
##              install them.
## Inputs     : None.
## Outputs    : None.
## Assumptions: That select has diverted print's to the makefile.
sub print_tooldeps
{
   my $tf;
   my @tools = sort glob("*.c");
   my @targets;

   if (!defined $host_compile_cmd)
   { die "Can't expand tool dependencies without recepie.\n"; }

   print "TOOLOBJ = ";
   foreach $tf (@tools)
   {
     my $re = $tf;

     $re =~ s/\.c$//;
     $re =  "\$(TOOLOUT)$path_sep$re\$(TOOLEXT)";

     push(@targets, { 'target' => $re, 'source' => $tf });
     print "$re ";
   }
   print "\n\n";

   foreach $tf (@targets)
   {
     my $target = $tf->{'target'};
     my $source = $tf->{'source'};
     my $cmd    = $host_compile_cmd;

     $cmd =~ s/__SRC__/$source/g;
     $cmd =~ s/__TGT__/$target/g;

     print "$target : $source \$(TOOLHDRS)\n";
     print "\t$cmd\n\n";
   }

   print "\n";
}

## Subroutine : print_perldeps
## Purpose    : Scan for "Installable Perl Files" and build a list
##              of targets and generate the appropriate makeage to
##              install them.
## Inputs     : None.
## Outputs    : None.
## Assumptions: That select has diverted print's to the makefile.
sub print_perldeps
{
   my $pf;
   my @perls = sort glob("*.ipl");
   my @targets;

   if (!defined $mktool_cmd)
   { die "Can't expand perl dependencies without recepie.\n"; }

   print "PERLOBJ = ";
   foreach $pf (@perls)
   {
     my $re = $pf;

     $re =~ s/\.ipl$//;
     $re =  "\$(TOOLOUT)$path_sep$re\$(PERLEXT)";

     push(@targets, { 'target' => $re, 'source' => $pf });
     print "$re ";
   }
   print "\n\n";

   foreach $pf (@targets)
   {
     my $target = $pf->{'target'};
     my $source = $pf->{'source'};
     my $cmd    = $mktool_cmd;

     $cmd =~ s/__SRC__/$source/g;
     $cmd =~ s/__TGT__/$target/g;

     print "$target : $source\n";
     print "\t$cmd\n\n";
   }

   print "\n";
}

## Subroutine : gen_makefile
## Purpose    : Read in a makefile template and search for a special
##              marker that indicates dependencies should be inserted,
##              and call print_deps() to supply them.  Write the
##              aggregate output to a finished makefile.
## Inputs     : None.
## Outputs    : None.
## Assumptions: Relies on global variables $mktmpl_name and $output_name.
sub gen_makefile
{
    my $depth = compute_depth(shift);
    my @pubincs;
    my @xincs;

    open(INPUT, $mktmpl_name)
        or return;

    open(OUTPUT, ">$output_name")
        or die "Can't open $output_name for writing: $!\n";

    select(OUTPUT);

    while (<INPUT>)
    {
       if (/^EXPAND\s+SRCDEPS\s*$/)
       { print_deps(\@pubincs, \@xincs); }
       elsif (/^EXPAND\s+PERLDEPS\s*$/)
       { print_perldeps(); }
       elsif (/^EXPAND\s+TOOLDEPS\s*$/)
       { print_tooldeps(); }
       elsif (/^EXPAND\s+DEPTH\s*$/)
       {  print "DEPTH = $depth\n";  }
       elsif (/^EXTRA_INCLUDE\s+(.*)\s*$/)
       { push(@xincs, split(/s+/, $1)); }
       elsif (/^PUBINCS\s+(.*)\s*$/)
       {  push(@pubincs, split(/\s+/, $1)); } 
       elsif (/^USE\s+(.*)\s*$/)
       {  
          my @u = split(/\s+/, $1);
          foreach (@u)
          {
            if (defined $rules{$_})
            { print $rules{$_};  }
            else
            { die "Unknown template $_."; }
          }
       }
       else
       {
          s/\:DIR\:/$path_sep/go;  
          print; 
       }
    }

    close(INPUT);
    close(OUTPUT);

    select(STDOUT);
}

## Subroutine : wanted
## Purpose    : Called during recursion through the target directory; 
##              calls gen_makefile() for each Makefile.gen found, or 
##              removes the generated makefile if the "-c" option is 
##              in effect.
## Inputs     : None.
## Outputs    : None.
## Assumptions: Depends on global $mktmpl_name, intended to be called from 
##              File::Find
sub wanted
{
    return unless (/^$mktmpl_name$/);

    if ($opt_c) 
    { unlink($output_name); }
    else
    { 
      my $fn = $_;

      # It seems that some versions of Perl are quite sensitive to having
      # $_ mucked with during a File::Find callback. In particular, this
      # showed up on bigmac running MkLinux/PPC. 
      #
      # So, let's just keep our $_ to our selvs.
      #
      # Why is Perl's File::Find so ugly?
      #
      local($_);
      gen_makefile($fn); 
    }
}

sub usage
{
    print "usage: $0 [-r file] [-c] -d <dir>\n";
    exit 0;
}

## main

getopts('cd:r:');

# the -d option is required.
if (! $opt_d) { usage(); }
if ($opt_r)   { read_rulelib($opt_r); }

find(\&wanted, $opt_d);



