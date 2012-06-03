#!/usr/bin/python

import sys, MySQLdb

(thisprog, logfile) = sys.argv
f = open(logfile)

# Swallow the headers
assert('started with' in f.readline())
assert(f.readline().startswith('TCP Port'))
assert(f.readline().startswith('Time '))

queries = []
for l in f.readlines():
  assert(l[-1] == '\n')
  l = l[:-1]
  assert(l[-1] != '\r')

  if l.startswith('\t\t'):
    ll = l[2:]
  else:
    ll = l[l.find('\t')+1:]
  id = ll[0:5].strip()
  rest = ll[6:]
  tabpos = rest.find('\t')
  assert(tabpos >= 0)

  cmd = rest[:tabpos]
  args = rest[tabpos+1:]
  queries.append((id, cmd, args))

cxns = {}
for (id, cmd, args) in queries:
  if cmd == 'Connect':
    cxns[id] = MySQLdb.connect(host='localhost', user='nickolai', passwd='', db='Director')
  elif cmd == 'Query':
    cursor = cxns[id].cursor()
    try:
      cursor.execute(args)
    except Exception, e:
      print 'Exception during query', args
      print e
    cursor.close()
  elif cmd == 'Quit':
    cxns[id].close()
    cxns[id] = None
  else:
    print 'Unknown command', cmd, args
