use strict;

if (scalar(@ARGV) != 1) {
  die "Usage: AnalyzeLatency.pl LP_A:Slot_A
  This script measures the latency between a start Godec point and all other activated points. Set the respective components' verbosity to true, save the log, and run the script on it";
}

my @elsA = split(/\:/,$ARGV[0]);
my $AName = $elsA[0];
my $ASlot = $elsA[1];
my $AAction = "Pushing";

my @eventList;
while(my $line = <STDIN>) {
  if ($line =~ /\([0-9]+\.[0-9]+\)/) {
    my ($prefix,$name,$time,$action,$slot,$streamTime) = ($line =~ m/^(.+)\ (.+)\(([0-9\.]+)\):\ ([^\s]+).+\[([^\]]+),([^\]]+)\]/);
    $name =~ s/^Toplevel->//ig;
    if (not ($prefix eq "LP" or $prefix eq "Submodule")) {next;} 
    if ($name eq "" or $time eq "" or $action eq "" or $slot eq "" or $streamTime eq "") {next;}
    if (not ($action eq "incoming" or $action eq "Pushing")) {next;}
    my %tmpHash;
    $tmpHash{"name"} = $name;
    $tmpHash{"realtime"} = $time;
    $tmpHash{"action"} = $action;
    $tmpHash{"slot"} = $slot;
    $tmpHash{"streamtime"} = $streamTime;
#   printEvent(\%tmpHash);
    push @eventList, \%tmpHash;
  }
}

my $realTimeOffset = 0;
my $lastATime = 0;
my %outStats;
my %tag2TotalLatency;
my @sortedEvents = sort {
  $a->{"streamtime"} <=> $b->{"streamtime"} || $a->{"realtime"} <=> $b->{"realtime"}
  } @eventList;
foreach my $event (@sortedEvents) {
#printEvent($event);
  if ($realTimeOffset == 0) {
    $realTimeOffset = $event->{"realtime"};
  }
  if ($event->{"name"} eq $AName and $event->{"action"} eq $AAction and $event->{"slot"} eq $ASlot) {
    $lastATime = $event->{"realtime"};
  }
  if ($lastATime != 0 and $event->{"action"} eq "Pushing" and !($event->{"name"} eq $AName) and ($event->{"slot"} !~ /convstate/i)) {
    my $tag = $event->{"name"}."[".$event->{"slot"}."]"; 
    my $latency = ($event->{"realtime"}-$lastATime); 
    $tag =~ s/->/\./g;
    $tag =~ s/_/\./g;
    push @{$outStats{$tag}}, "".($event->{"realtime"}-$realTimeOffset)." ".$latency;
    $tag2TotalLatency{$tag} += $latency;
  }
}

foreach my $tag (sort {$tag2TotalLatency{a} <=> $tag2TotalLatency{$b}} keys(%tag2TotalLatency)) {
  print $tag."\n";
  foreach my $line (@{$outStats{$tag}}) {
    print $line."\n";
  }
  print "\n\n";
}

sub printEvent {
  my $event = shift;
  print "Event: ".$event->{"name"}.",".$event->{"streamtime"}.",".$event->{"slot"}.",".$event->{"realtime"}.",".$event->{"action"}."\n";
}
