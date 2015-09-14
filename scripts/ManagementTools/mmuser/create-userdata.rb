#!/usr/bin/env ruby
# encoding: UTF-8

# generate accounts info with yaml format.
# account attributes are retrieved from stdin (passwd format)
# password is replaced to username
# Usage: $0 user [user ...] < /etc/passwd

require 'yaml'

MIN_UID=500
MIN_GID=500
DEFAULT_GROUPS="dpdk,ramcloud"

# $6$: SHA-512. see crypt(3) for details.
SALT = "$6$YyZ..kqKnWIF9Vm/"

accounts = Hash.new
$stdin.readlines.each do |line|
  next if line =~ /^\s*#/
  entry = line.strip.split(':')
  uinfo = { 'name'   => entry[0], 'uid'  => entry[2], 'gid'   => entry[3],
            'gecos'  => entry[4], 'home' => entry[5], 'shell' => entry[6],
            'groups' => entry[7], }
  next if uinfo['name'].empty?
  next if uinfo['uid'].to_i < MIN_UID
  next if uinfo['gid'].to_i < MIN_GID
  uinfo['gid'] = uinfo['uid'] if uinfo['gid'].empty?
  uinfo['groups'] = if uinfo['groups'].to_s.empty? then
                       DEFAULT_GROUPS else
                      [DEFAULT_GROUPS, uinfo['groups']].join(',') end

  uinfo['home']   = "/home/" + uinfo['name'] if uinfo['home'].empty?
  uinfo['shell']  = '/bin/bash' if uinfo['shell'].empty?
  uinfo['passwd'] = uinfo['name'].crypt(SALT)
  accounts[uinfo['name'].to_sym] = uinfo
end

users = Array.new
ARGV.each do |user|
  next unless accounts.has_key?(user.to_sym)
  users << accounts[user.to_sym]
end

puts ({ 'accounts' => users, }).to_yaml
