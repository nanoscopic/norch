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

part::reqrep::dofork();
part::datastore::dofork();
part::results::dolisten();
part::scheduler::dolisten();
exit(0);