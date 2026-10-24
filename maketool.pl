#! /usr/bin/perl
##---------------------------------------------------------------------------
##
## MODULE NAME:     maketool.pl
##
## COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
##                  file within the Seminole distribution for terms of use.
##                  If your distribution is incomplete, the Seminole
##                  documentation and license are available on the World
##                  Wide Web at http://www.conman.org/software/seminole/
##
## DESCRIPTION:     Perl tool installer
##
## AUTHOR:          M. Grosberg
##
## CREATION DATE:   Jan 24, 2002
##
##---------------------------------------------------------------------------
##
## HISTORY OF CHANGE
##
## -----+--------+------------------------------------------+------+---------
## VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
## =====+========+==========================================+======+=========
##  001 |01/24/02| File creation.                           | MYG  |
## -----+--------+------------------------------------------+------+---------
##  002 |07/11/02| Disable cygwin Perl.                     | MYG  | S1005
## -----+--------+------------------------------------------+------+---------
##---------------------------------------------------------------------------

use strict;
use Getopt::Std;
use Cwd 'abs_path';

# from Getopt::Std
use vars qw($opt_p $opt_t);

my $is_pc = (
                 ($^O eq 'dos')
              || ($^O eq 'MSWin32')
              || ($^O eq 'os2')
              || ($^O eq 'cygwin')
            );
my $path_sep = ($is_pc ? "\\" : "/");

sub main
{
  # Parse arguments.
  getopts('p:t:');

  $opt_t = abs_path($opt_t);
  
  die "Cygwin Perl is no longer supported.\n" if ($^O eq 'cygwin');

  # Now quote certain special characters to prevent Perl from barking.
  $opt_t =~ s/([\'\\\"\r\n])/\\$1/g;

  if (scalar(@ARGV) != 2)
  {
    print STDERR "Usage:\n\t$0 -p <perl> -t <tooldir> "
               . "<outfile> <infile>\n";
    exit 1;
  }

  my $df = $ARGV[1];
  my $sf = $ARGV[0];

  my @tp = split(/\Q$path_sep\E/, $df);
  my $tn = pop(@tp);
 
  print "Installing tool $tn\n";

  if ($is_pc)
  { do_pc($sf, $df); }
  else
  { do_unix($sf, $df); }
}

sub copy_contents
{
  while (<SRC>)
  { 
     if (defined $opt_t)
     { s/__TOOL_DIR__/$opt_t/g; }
     print DST $_; 
  }
}

sub do_unix
{
  my ($sf, $df) = @_;

  # If not supplied, guess.
  $opt_p ||= $^X;

  open(SRC, $sf) or 
    die "Can't open input file $sf\n";
  open(DST, '>' . $df) or 
    die "Can't open output file $df\n";

  # Add the shebang.
  print DST "#! $opt_p\n";

  # Copy the remainder
  copy_contents();

  # Close the files.
  close(SRC);
  close(DST);

  chmod(0755, $df);
}

sub do_pc
{
  my ($sf, $df) = @_;

  open(SRC, $sf) or 
    die "Can't open input file $sf\n";
  open(DST, '>' . $df) or 
    die "Can't open output file $df\n";

  # Copy the remainder
  copy_contents();

  # Close the files.
  close(SRC);
  close(DST);
}

main();
