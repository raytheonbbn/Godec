use Cwd;
use strict;
use File::Basename;

if (scalar(@ARGV) != 2) {die "Usage: perl GenDoc.pl <csv list of directories with components source code> <comma-separated list of directories containing slot definitions";}
my @scanDirs = split(/,/,$ARGV[0]);
my $slotDefs = GetSlotDefs($ARGV[1]);
my @filesToScan;
foreach my $scanDir (@scanDirs) {
  push @filesToScan, glob($scanDir."/*.h*");
}
@filesToScan = sort @filesToScan;

sub GetSlotDefs {
  my $dirs = shift;
  my %slotDefs;
  foreach my $dir (split(/,/,$dirs)) {
    my @files = grep /\.[c|h]/, glob($dir."/*.*");
    foreach my $file (@files) {
      open(IN, $file) || die "Can't open ".$file;
      while(my $line = <IN>) {
        if ($line =~ /std\:\:string\s+.*\:?\:?Slot.+=\s*\"/) {
          my ($slotVar, $slot) = ($line =~ m/(Slot[^\s]+)\s*=\s*\"(.+)\"/);
        $slotDefs{$slotVar} = $slot;
      }
      }
    }
  }
  return \%slotDefs;
}

sub MDEscape {
  my $in = shift;
  $in =~ s/_/\\_/g;
  return $in;
}

sub ScanFile {
  my $fileBase = shift;
  my $path = shift;
  my $slotDefs = shift;
  my @hFiles = glob($path."/".$fileBase.".h*");
  if (scalar(@hFiles) == 0) {die "No matching .h* file found";}
  my $hFile = $hFiles[0];
  open(HIN,"<$hFile") || die "Can't open ".$hFile;
  my %outHash;
  while(my $line = <HIN>) {
    my ($className) = ($line =~ m/^\s*class\s+([^\s]+)\s*:\s*public\s+LoopProcessor/);
    if ($className ne "") {
      my $prettyName = $className;
      $prettyName =~ s/Component//g;
      $outHash{"name"} = $prettyName;
      my @cFiles = glob($path."/".$fileBase.".c*");
      if (scalar(@cFiles) == 0) {die "No matching .c* file found";}
      my $cFile = $cFiles[0];
      open(CIN,"<$cFile") || die "Can't open ".$cFile;
      while(my $line = <CIN>) {
        if ($line =~ /$className\:\:describeThyself/) {
          my $descLine;
          do {
            $descLine = <CIN>;
            if ($descLine =~ /^\s*return/) {
              my ($text) = ($descLine =~ m/return\ \"(.+)\"/);
              $outHash{"short_description"} = $text;
            }
          }while ($descLine !~ /^\s*}/);
        }
      }
      seek CIN, 0, 0;
      while(my $line = <CIN>) {
        if ($line =~ /$className\:\:ExtendedDescription/) {
          my $extendedDescription;
          my $descLine;
          my $seenEnd = 0;
          do {
            $seenEnd = 0;
            $descLine = <CIN>;
            chomp($descLine);
            if ($descLine =~ /\*\//) {
              $seenEnd = 1;
            }
            $descLine =~ s/^\s+//g;
            $descLine =~ s/\s+$//g;
            $descLine =~ s/\*\///g;
            $extendedDescription .= $descLine."  \n";
          }while ($seenEnd == 0);
          $outHash{"extended_description"} = $extendedDescription;
        }
      }
      seek CIN, 0, 0;
      while(my $line = <CIN>) {
        if ($line =~ /$className\:\:.+\(.*ComponentGraphConfig\ *\*/) {
          my $bracketCounter = ($line =~ m/({)/g);
          while (1) {
            my $constructorLine = <CIN>;
            if ($constructorLine =~ /GodecDocIgnore/i) {next;}
            my @brackets = ($constructorLine =~ m/([{}])/g);
            for my $bracket (@brackets) {
              if ($bracket eq "{") {$bracketCounter++;}
              elsif ($bracket eq "}") {$bracketCounter--;}
            }
            if ($constructorLine =~ /configPt->get\</) {
              my ($type,$varName,$desc) = ($constructorLine =~ m/configPt->get<([^\>]+)>\(\"(.+)\",\s*\"(.+)\"/);
              $type =~ s/std\:\://g;
              $outHash{"params"}->{$varName}->{"type"} = $type;
              $outHash{"params"}->{$varName}->{"description"} = $desc;
            }
            if ($constructorLine =~ /addInputSlotAndUUID/) {
              my ($slot,$msgType) = ($constructorLine =~ m/addInputSlotAndUUID\((.+)\s*,\s*(.+)\)/);
              $msgType =~ s/UUID_//g;
              if (defined($slotDefs->{$slot})) {
                push @{$outHash{"inputs"}->{$slotDefs->{$slot}}},$msgType;
              } else {
                push @{$outHash{"inputs"}->{$slot}},$msgType;
              }
            }
            if ($constructorLine =~ /\.push_back\(Slot/) {
              my ($slot) = ($constructorLine =~ m/push_back\((.+)\)/);
              if (defined($slotDefs->{$slot})) {
                $outHash{"outputs"}->{$slotDefs->{$slot}} = 1;
              } else {
                $outHash{"outputs"}->{$slot} = 1;
              }
            }
          } continue { last unless ($bracketCounter > 0) };
        }
      }
      close(CIN);
    }
  }
  close(HIN);
  return \%outHash;
}

my @components;
foreach my $fileToScan (@filesToScan) {
  if ($fileToScan =~ /ApiEndpoint/) {
    next;
  }
  my ($className,$path,$suffix) = fileparse($fileToScan,qr/\.[^.]*/);
  my $hashRef = ScanFile($className, $path, $slotDefs);
  push @components, $hashRef;
}

print "# Godec core component list\n\n";
for my $component (sort {$a->{"name"} cmp $b->{"name"}} @components) {
  if (defined $component->{"name"}) {
    my $name = $component->{"name"}; 
    print "[".$name."](#".lc($name).")  \n";
  }
}

for my $component (sort {$a->{"name"} cmp $b->{"name"}} @components) {
  if (defined $component->{"name"}) {
    print "\n\n## ".MDEscape($component->{"name"})."\n\n---\n\n";
    print "### Short description:\n".MDEscape($component->{"short_description"})."\n\n";
    if (defined($component->{"extended_description"})) {
      print "### Extended description:\n".$component->{"extended_description"}."\n\n";
    }
    if (defined $component->{"params"}) {
      print "#### Parameters\n";
      print "| Parameter | Type | Description |\n";
      print "| --- | --- | --- |\n";
      for my $varName (sort keys(%{$component->{"params"}})) {
        my $type = $component->{"params"}->{$varName}->{"type"};
        my $desc = $component->{"params"}->{$varName}->{"description"};
	print "| ".MDEscape($varName)." | ".MDEscape($type)." | ".MDEscape($desc)." |\n";
      }
      print "\n";
    }
    if (defined $component->{"inputs"}) {
      my %inputs = %{$component->{"inputs"}};
      print "#### Inputs\n";
      print "| Input slot | Message Type | \n";
      print "| --- | --- | \n";
      for my $slot (sort keys(%inputs)) {
        print "| ".MDEscape($slot)." | ".MDEscape(join(",",@{$inputs{$slot}}))."|\n";
      }
      print "\n";
    }
    if (defined $component->{"outputs"}) {
      my %outputs = %{$component->{"outputs"}};
      print "#### Outputs\n";
      print "| Output slot | \n";
      print "| --- | \n";
      for my $slot (sort keys(%outputs)) {
        print "| ".MDEscape($slot)." | \n";
      }
    }
  }
}
  
