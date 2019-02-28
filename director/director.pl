#!/usr/bin/perl -w

# Nanomsg Orchestration System
# Copyright (C) SUSE LLC 2019

use strict;
use NanoMsg::Raw;
use Time::HiRes qw/gettimeofday tv_interval/;
use Parse::XJR;
use Data::Dumper;
use lib '.';
use part::datastore;
use part::reqrep;
use part::results;
use part::scheduler;

part::reqrep::dofork();
part::datastore::dofork();
part::scheduler::dofork();
part::results::dolisten();

exit(0);