#!/usr/bin/perl
#
# Hacky replacement for (non-existant) svn-config to find the libraries,
# the library path and the include file path.

use strict;

my $libs = 0;
my $includes = 0;

for( my $i = 0; $i < ($#ARGV + 1); $i++ ) {
   if( $ARGV[$i] =~ /link-ld/ ) { $libs = 1; }
   if( $ARGV[$i] =~ /includes/ ) { $includes = 1; }
}

my $SVN = `which svn`;
my $LDD = `which ldd`;

chomp($SVN); chomp($LDD);

my $out_ld_path = "";
my $out_libs = "";
my $out_includes = "";
my $lib = "";

if( $libs ) {
   open(IN, "$LDD $SVN |");
   while( <IN> ) {
      chomp();
      if( /svn/ && 
         (/client/ || /wc/ || /repos/) ) {

         if( !$out_ld_path ) {
            $out_ld_path = $_;
            $out_ld_path =~ s/^.*?=> ([\/a-z0-9_-]+)?\/.*/$1/;
            $out_ld_path = "-L".$out_ld_path;
         }

         $lib = $_;
         $lib =~ s/(\s+)([a-z0-9_-]+)(.*)/$2/;
         $lib =~ s/lib/-l/;
         $out_libs = $out_libs.$lib." ";
      }
   }
   close(IN);
   print $out_ld_path." ".$out_libs;
}

if( $includes ) {
   open(IN, "ls -d /usr/include/subversion* 2>/dev/null |");
   while( <IN> ) {
      chomp();
      $out_includes = $out_includes."-I".$_." ";
   }
   close(IN);
   print $out_includes;
}

print "\n";
