
TEMPLATE = subdirs
SUBDIRS = sub_checker_lib sub_checker_client sub_helper

sub_checker_lib.file = checker-lib.pro
sub_checker_client.file = checker-client.pro
sub_helper.file = helper.pro

CONFIG += ordered
