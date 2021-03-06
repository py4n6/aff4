#!/usr/bin/env python
# ******************************************************
# Copyright 2004: Commonwealth of Australia.
#
# Developed by the Computer Network Vulnerability Team,
# Information Security Group.
# Department of Defence.
#
# Michael Cohen <scudette@users.sourceforge.net>
#
# ******************************************************
#  Version: FLAG  $Version: 0.87-pre1 Date: Thu Jun 12 00:48:38 EST 2008$
# ******************************************************
#
# * This program is free software; you can redistribute it and/or
# * modify it under the terms of the GNU General Public License
# * as published by the Free Software Foundation; either version 2
# * of the License, or (at your option) any later version.
# *
# * This program is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# * You should have received a copy of the GNU General Public License
# * along with this program; if not, write to the Free Software
# * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
# ******************************************************

""" This module contains db related functions """
import MySQLdb,re
import MySQLdb.cursors
import _mysql
import pyflag.conf
config = pyflag.conf.ConfObject()

import pyflag.pyflaglog as pyflaglog
import time,types
from Queue import Queue, Full, Empty
from MySQLdb.constants import FIELD_TYPE, FLAG
import threading

## This store stores information about indexes
import Store
DBIndex_Cache=Store.Store()

db_connections=0
mysql_connection_args = None

## Declare the configuration parameters we need here:
config.add_option("FLAGDB", default='pyflag',
                help="Default pyflag database name")

config.add_option("DBUSER", default='root',
                help="Username to connect to db with")

config.add_option("DBPASSWD", default=None,
                help="Password to connect to the database")

config.add_option("STRICTSQL", default=False, action='store_true',
                metavar = "true/false",
                help="database warnings are fatal")

config.add_option("DBHOST", default='localhost',
                help="database host to connect to")

config.add_option("DBPORT", default=3306, type='int',
                help="database port to connect to")

config.add_option("DBUNIXSOCKET", default="/var/run/mysqld/mysqld.sock",
                help="path to mysql socket")

config.add_option("DBCACHE_AGE", default=60, type='int',
                help="The length of time table searches remain cached")

config.add_option("DBCACHE_LENGTH", default=1024, type='int',
                help="Number of rows to cache for table searches")

config.add_option("MASS_INSERT_THRESHOLD", default=300, type='int',
                  help="Number of rows where the mass insert buffer will be flushed.")

config.add_option("TABLE_QUERY_TIMEOUT", default=10, type='int',
                  help="The table widget will timeout queries after this many seconds")

config.add_option("SQL_CACHE_MODE", default="realtime",
                  help="The SQL Cache mode (can be realtime, periodic, all) see the wiki page for explaination")

config.add_option("DB_SS_CURSOR", default=False,
                  action='store_true',
                  help = "Enable server side database cursors")

import types
import MySQLdb.converters

conv = MySQLdb.converters.conversions.copy()

## Remove those conversions which we do not want:
del conv[FIELD_TYPE.TIMESTAMP]
del conv[FIELD_TYPE.DATETIME]
del conv[FIELD_TYPE.DECIMAL]
del conv[FIELD_TYPE.NEWDECIMAL]
del conv[FIELD_TYPE.TIME]
del conv[FIELD_TYPE.DATE]
del conv[FIELD_TYPE.YEAR]

class DBError(Exception):
    """ Generic Database Exception """
    pass

def force_unicode(string):
    """ Make sure the string is unicode where its supposed to be """
    if isinstance(string, str):
        try:
            return unicode(string)
        except:
            #print "String %r should be unicode" % string
            pass
        
        return unicode(string, "utf-8", "ignore")

    return unicode(string)

def force_string(string):
    if isinstance(string, unicode):
        return string.encode("utf-8","ignore")
    return str(string)

expand_re = re.compile("%(r|s|b)")

def expand(format, params):
    """ This version of the expand function returns a unicode object
    after properly expanding the params into it.

    NOTE The Python % operator is evil - do not use it because it
    forces the decoding of its parameters into unicode when any of the
    args are unicode (it upcasts them). This is a very unsafe
    operation to automatically do because the default codec is ascii
    which will raise when a string contains a non ascii char. This is
    basically a time bomb which will blow when you least expect it.

    Never use %, use this function instead. This function will return
    a unicode object.
    """
    format = unicode(format)
    d = dict(count = 0)

    if isinstance(params, basestring):
        params = (params,)

    try:
        params[0]
    except:
        params = (params,)

    def cb(m):
        x = params[d['count']]
        
        if m.group(1)=="s":
            result = u"%s" % (force_unicode(params[d['count']]))
            
        ## Raw escaping
        elif m.group(1)=='r':
            result = u"'%s'" % escape(force_unicode(params[d['count']]), quote="'")

        d['count'] +=1
        return result

    result = expand_re.sub(cb,format)

    return result

def db_expand(sql, params):
    """ A utility function for interpolating into the query string.
    
    This class implements the correct interpolation so that %s is not escaped and %r is escaped in the mysql interpolation.

    @Note: We cant use the MySQLdb native interpolation because it has a brain dead way of interpolating - it always escapes _ALL_ parameters and always adds quotes around anything. So for example:

    >>> MySQLdb.execute('select blah from %s where id=%r',table,id)

    Does not work as it should, since the table name is always enclosed in quotes which is incorrect.

    NOTE: expand always returns a string not a unicode object because
    we might need to mix different encodings - for example we might
    have some binary data and utf8 strings mixed together in the
    return string. If we returned a unicode string it will be encoded
    to utf8 when sending to the server and the binary data will be
    corrupted.
    """
    sql = str(sql)
    d = dict(count = 0)

    if isinstance(params, basestring):
        params = (params,)

    try:
        params[0]
    except:
        params = (params,)

    def cb(m):
        x = params[d['count']]
        
        if m.group(1)=="s":
            #result = u"%s" % (force_unicode(params[d['count']]))
            result = force_string(x)
            
        ## Raw escaping
        elif m.group(1)=='r':
            result = force_string(x)
            result = "'%s'" % escape(result, quote="'")
            #result = u"'%s'" % escape(force_unicode(params[d['count']]), quote="'")
            #result = u"'%s'" % escape(force_unicode(params[d['count']]))

        ## This needs to be binary escaped:
        elif m.group(1)=='b':
            #data = params[d['count']].decode("latin1")
            data = params[d['count']]
            #data = ''.join(r"\\x%02X" % ord(x) for x in params[d['count']])
            #data = params[d['count']]
            result = "_binary'%s'" % escape(data)
            #print result.encode("ascii","ignore")

        d['count'] +=1
        return result

    result = expand_re.sub(cb,sql)

    return result

class PyFlagDirectCursor(MySQLdb.cursors.DictCursor):
    ignore_warnings = False
    
    def execute(self, string):
        pyflaglog.log(pyflaglog.VERBOSE_DEBUG, string)
        return MySQLdb.cursors.DictCursor.execute(self, string)
    
    def _warning_check(self):
        last = self._last_executed
        if self._warnings and not self.ignore_warnings:
            self.execute("SHOW WARNINGS")
            while 1:
                a=self.fetchone()
                if not a: break
                pyflaglog.log(pyflaglog.WARNINGS,"query %r: %s" % (last[:100],a['Message']))


class PyFlagCursor(MySQLdb.cursors.SSDictCursor):
    """ This cursor combines client side and server side result storage.

    We store a limited cache of rows client side, and fetch rows from
    the server when needed.
    """
    ignore_warnings = False
    logged = True
    
    def __init__(self, connection):
        MySQLdb.cursors.SSDictCursor.__init__(self, connection)
        self.py_row_cache = []
        ## Maximum size of client cache
        self.py_cache_size = 10
        self._last_executed = None
        self._last_executed_sequence = []

        ## By default queries are allowed to take a long time
        self.timeout = 0

    def kill_connection(self, what=''):
        dbh = DBO()
        try:
            dbh.execute("kill %s %s" % (what,self.connection.thread_id()))
        except:
            pass

    def execute(self,string):
        self.py_row_cache = []
        self.py_cache_size = 10
        self._last_executed = string
        self._last_executed_sequence.append(string)
        self._last_executed_sequence = self._last_executed_sequence[:-3]

        def cancel():
            pyflaglog.log(pyflaglog.WARNINGS, "Killing query in thread %s because it took too long" % self.connection.thread_id())
            self.kill_connection('query')

        if self.timeout:
            t = threading.Timer(self.timeout, cancel)
            t.start()
            try:
                pyflaglog.log(pyflaglog.VERBOSE_DEBUG, string)
                MySQLdb.cursors.SSDictCursor.execute(self,string)
            finally:
                t.cancel()
                t.join()
                pass
        else:
            if self.logged:
                pyflaglog.log(pyflaglog.VERBOSE_DEBUG, string)
            MySQLdb.cursors.SSDictCursor.execute(self,string)

    def fetchone(self):
        """ Updates the row cache if needed otherwise returns a single
        row from it.
        """
        self._check_executed()
        if len(self.py_row_cache)==0:
            self.py_row_cache = list(self._fetch_row(self.py_cache_size))

        try:
            result = self.py_row_cache.pop(0)
            self.rownumber = self.rownumber + 1
            return result
        except IndexError:
            return None

    def close(self):
        self.connection = None

    def _warning_check(self):
        """ We need to override this because for some cases it issues
        a SHOW WARNINGS query. Which will raise an 'out of sync
        error' when we operate in SS. This is a most sane approach -
        when warnings are detected, we simply try to drain the
        resultsets and then read the warnings.
        """
        if self.ignore_warnings: return
        
        ## We have warnings to show
        if self._warnings:
            last_executed = [ x[:500] for x in self._last_executed_sequence]

            results = list(self._fetch_row(1000))
            if len(results)<1000:
                self.execute("SHOW WARNINGS")
                while 1:
                    a=self.fetchone()
                    if not a: break
                    pyflaglog.log(pyflaglog.DEBUG,"Mysql warnings: query %r: %s" % (last_executed,a))
                else:
                    pyflaglog.log(pyflaglog.DEBUG,"Mysql issued warnings but we are unable to drain result queue")

            ## If we have strict SQL we abort on warnings:
            if config.STRICTSQL:
                raise DBError(a)

            self.py_row_cache.extend(results)

def mysql_connect(case):
    """ Connect specified case and return a new connection handle """
    global db_connections, mysql_connection_args

    ## If we already know the connection args we just go for it
    if mysql_connection_args:
        mysql_connection_args['db'] = case
        dbh=MySQLdb.Connection(**mysql_connection_args)
        dbh.autocommit(True)
        return dbh

    mysql_connection_args = dict(user = config.DBUSER,
                db = case,
                host=config.DBHOST,
                port=config.DBPORT,
                conv = conv,
                use_unicode = True,
                charset='utf8'
                )

    if config.DBPASSWD:
        mysql_connection_args['passwd'] = config.DBPASSWD

    if config.STRICTSQL:
        mysql_connection_args['sql_mode'] = "STRICT_ALL_TABLES"

    if config.DB_SS_CURSOR:
        mysql_connection_args['cursorclass'] = PyFlagCursor
    else:
        mysql_connection_args['cursorclass'] = PyFlagDirectCursor
        mysql_connection_args['cursorclass']._last_executed_sequence = []
        
    try:
        #Try to connect over TCP
        dbh = MySQLdb.Connect(**mysql_connection_args)
    except Exception,e:
        ## or maybe over the socket?
        mysql_connection_args['unix_socket'] = config.DBUNIXSOCKET
        del mysql_connection_args['host']
        del mysql_connection_args['port']

        dbh = MySQLdb.Connect(**mysql_connection_args)

    dbh.autocommit(True)

    return dbh

class Pool(Queue):
    """ Pyflag needs to maintain multiple simulataneous connections to
    the database sometimes.  To avoid having to reconnect on each
    occasion, we keep a pool of connection objects. This allows
    connections to be placed back in the pool after DBO destruction
    for reuse by other DBOs.
    
    Note that since we use the Queue class we are thread safe
    here. I.e. we guarantee that the same connection will never be
    shared by two different threads.

    Note that connections are not placed back on the queue until all
    references to them are GCed. This means that we still need to be
    relatively concervative in creating new DBOs and prefer to reuse
    existing ones whenever possible (of course we need to create new
    ones if the connection state is important (e.g. iterating through
    a resultset while doing some different db activity).

    The pool maintains a cache of case parameters, which we get from
    the meta table. If these change, The cache needs to be expired.
    """
    def __init__(self, case, poolsize=0):
        self.case=case
        self.indexes = {}
        self._parameters = {}
        Queue.__init__(self, poolsize)

    def parameter(self, string):
        try:
            return self._parameters[string]
        except KeyError:
            dbh = self.get()
            c=dbh.cursor()

            c.execute("select value from meta where property = %r limit 1" % string)
            row = c.fetchone()
            try:
                return row['value']
            except:
                return None

    def parameter_flush(self):
        """ Expire the parameter cache """
        self._parameters = {}

    def put(self, dbh):
        pyflaglog.log(pyflaglog.VERBOSE_DEBUG, "Returning dbh to pool %s" % self.case)
        Queue.put(self,dbh)

    def get(self, block=1):
        """Get an object from the pool or a new one if empty."""
        try:
            try:
                result=self.empty() and mysql_connect(self.case) or Queue.get(self, block)

                pyflaglog.log(pyflaglog.VERBOSE_DEBUG, "Getting dbh from pool %s" % self.case)
            except Empty:
                result = mysql_connect(self.case)

            return result
        except Exception,e:
            ## We just failed to connect - i bet the cached variables
            ## are totally wrong - invalidate the cache:
            global mysql_connection_args
            
            mysql_connection_args = None            
            raise DBError("Unable to connect - does the DB Exist?: %s" % e)

class PooledDBO:
    """ Class controlling access to DB handles

    We implement a pool of connection threads. This gives us both worlds - the advantage of reusing connection handles without running the risk of exhausting them, as well as the ability to issue multiple simultaneous queries from different threads.

    @cvar DBH: A store containing cached database connection objects
    @cvar lock: an array of unique locks that each thread must hold before executing new SQL
    @ivar temp_tables: A variable that keeps track of temporary tables so they may be dropped when this object gets gc'ed
    """
    temp_tables = []
    transaction = False
    ## This stores references to the pools
    DBH = Store.Store(max_size=10)

    def get_dbh(self, case):
        try:
            pool = self.DBH.get(case)
        except KeyError:
            pool = Pool(case)
            self.DBH.put(pool, key=case)
        
        self.dbh = pool.get()

        ## FIXME - the TZ should be set when the dbh is connected.
        return
        ## Ensure the tz is properly reset before handing out dbh:
        c = self.dbh.cursor()
        try:
            c.execute("select value from meta where property ='TZ' limit 1")
            row = c.fetchone()
            if row:
                c.execute(db_expand('set time_zone = %r', row['value']))
        except Exception,e:
            pass

    def __init__(self,case=None):
        """ Constructor for DB access. Note that this object implements database connection caching and so should be instantiated whenever needed. If case is None, the handler returned is for the default flag DB

        @arg case: Case database to connect to. May be None in which case it connects to the default flag database
        """
        if not case:
            case = config.FLAGDB

        self.get_dbh(case)
        self.temp_tables = []
        self.case = case
        self.cursor = self.dbh.cursor()
        self.tranaction = False

    def start_transaction(self):
        self.execute("start transaction")
        self.tranaction = True

    def end_transaction(self):
        self.execute("commit")
        self.tranaction = False
        
    def clone(self):
        """ Returns a new database object for the same case database """
        return self.__class__(self.case)

    def execute(self,query_str, *params):
        """  SQL execution method.
               This functions executes the SQL in this object's cursor context. the query must be given as a string with with %s or %r escape characters, and the correct number of strings in the params list.

               @note: Just as a reminder - using %r will escape the corresponding string in a manner that is adequate for mysql, it will also automatically insert quotes around the string. On the other hand using %s will not escape the strings.
               
               >>> a.execute('select * from %s where id=%r' , ('table','person'))
                       
               @arg query_str: A format string with only %r and %s format sequences
               @arg params: A list of strings which will be formatted into query_str. If there is only one format string and the programmer is truely lazy, a string is ok. """
        try:
            params[0].__iter__
            params = params[0]
        except (AttributeError,IndexError):
            pass

        if params:
            string = db_expand(query_str, params)
        else: string = query_str
        try:
            self.cursor.execute(string)
        #If anything went wrong we raise it as a DBError
        except Exception,e:
            str = "%s" % e
            if     'cursor closed' in str or \
                   'Commands out of sync' in str or \
                   'server has gone away' in str or \
                   'Lost connection' in str:
                pyflaglog.log(pyflaglog.VERBOSE_DEBUG,
                            "Got DB Error: %s" % (str))

                ## We terminate the current connection and reconnect
                ## to the DB
                pyflaglog.log(pyflaglog.DEBUG, "Killing connection because %s. Last query was %s" % (e,self.cursor._last_executed_sequence))

                try:
                    self.cursor.kill_connection()
                    del self.dbh
                except AttributeError:
                    pass
                
                global db_connections
                db_connections -=1
                self.get_dbh(self.case)
                #self.dbh.ignore_warnings = self.cursor.ignore_warnings
                
                self.cursor = self.dbh.cursor()

                ## Redo the query with the new connection - if we fail
                ## again, we just raise - otherwise we risk running
                ## into recursion issues:
                return self.cursor.execute(string)
                
            elif not str.startswith('Records'):
                raise DBError(e)

    def expire_cache(self):
        """ Expires the cache if needed """
        ## Expire the cache if needed
        self.start_transaction()
        try:
            self.execute("select * from sql_cache where timestamp < date_sub(now(), interval %r minute) for update", config.DBCACHE_AGE)
            ## Make a copy to maintain the cursor
            tables = [ row['id'] for row in self ]
            for table_id in tables:
                self.execute("delete from sql_cache where id = %r" , table_id)
                self.execute("delete from sql_cache_tables where sql_id = %r" , table_id)
                self.execute("drop table if exists `cache_%s`" , table_id)
                
        finally:
            self.end_transaction()
            
    def cached_execute(self, sql, limit=0, length=50):
        """ Executes the sql statement using the cache.

        strategy: Check the sql_cache table for the row, if it does not exist, make it
        if its in progress, spin for a while
        if its cached return it.
        
        """
        if config.SQL_CACHE_MODE=="realtime":
            self.expire_cache()
            
        self.execute("""select * from sql_cache where query = %r and `limit` <= %r and `limit` + `length` >= %r order by id limit 1 for update""", (sql, limit, limit + length))
        row = self.fetch()
        if not row:
            ## Row does not exist - make it
            return self._make_sql_cache_entry(sql, limit, length)
        elif row['status'] == 'dirty':
            if config.SQL_CACHE_MODE == "realtime":
                ## Row is dirty - make it
                self._make_sql_cache_entry(sql, limit, length)
            else:
                ## We dont care about dirty rows now
                return self.execute("select * from cache_%s limit %s,%s",
                                    (row['id'], limit - row['limit'], length))
            
        elif row['status'] == 'cached':
            return self.execute("select * from cache_%s limit %s,%s",
                                (row['id'], limit - row['limit'], length))
        elif row['status'] == 'progress':
            ## Spin until its ready
            count = config.TABLE_QUERY_TIMEOUT
            while count > 0:
                self.execute("select * from sql_cache where id=%r and status!='progress' limit 1", row['id'])
                row2=self.fetch()
                if row2:
                    return self.execute("select * from cache_%s limit %s,%s",
                                        (row['id'], limit - row['limit'], length))

                time.sleep(1)
                count -= 1
                pyflaglog.log(pyflaglog.DEBUG,"Waiting for query to complete for %u" % count)
            raise DBError("Query still executing - try again soon")

    def _make_sql_cache_entry(self, sql, limit, length):
        ## Query is not in cache - create a new cache entry: We create
        ## the cache centered on the required range - this allows
        ## quick paging forward and backwards.
        lower_limit = max(limit - config.DBCACHE_LENGTH/2,0)

        ## Determine which tables are involved:
        self.execute("explain %s", sql)
        tables = [ row['table'] for row in self if row['table'] ]
            
        if not tables:
            ## Should not happen - the query does not affect any tables??
            return self.execute("%s limit %s,%s",sql, limit, length)

        self.insert('sql_cache',
                    query = sql, _timestamp='now()',
                    limit = lower_limit,
                    length = config.DBCACHE_LENGTH,
                    status = 'progress',
                    _fast = True
                    )

        ## Create the new table
        id = self.autoincrement()
            
        ## Store the tables in the sql_cache_tables:
        for t in tables:
            self.insert('sql_cache_tables',
                        sql_id = id,
                        table_name = t,
                        _fast = True)
            
        ## Now comes the tricky part
        def run_query():
            dbh = DBO(self.case)
            dbh.discard = True
            try:
                dbh.execute("create table cache_%s %s limit %s,%s",
                            (id,sql, lower_limit, config.DBCACHE_LENGTH))
                for row in dbh: pass
                dbh.execute("update sql_cache set status='cached' where id=%r" , id)
                dbh.execute("commit")
            except Exception,e:
                ## Make sure we remove the progress status from the
                ## table
                dbh.execute("delete from sql_cache where id=%r", id)
                raise
            
        ## We start by launching a worker thread
        worker = threading.Thread(target=run_query)
        worker.start()

        ## Wait for the worker to finish
        worker.join(config.TABLE_QUERY_TIMEOUT)
        if worker.isAlive():
            raise DBError("Query still executing - try again soon")

        for i in range(config.TABLE_QUERY_TIMEOUT):
            try:
                return self.execute("select * from cache_%s limit %s,%s",
                                    (id,limit - lower_limit,length))
            except DBError,e:
                ## Sometimes it takes a while for the table to be
                ## available to the two threads. (Is this normal -
                ## this seems like a weird race?)
                if "doesn't exist" in e.__str__():
                    time.sleep(1)
                else:
                    break

                raise e
            
    def __iter__(self):
        return self

    def invalidate(self,table):
        """ Invalidate all copies of the cache which relate to this table """
        if config.SQL_CACHE_MODE=='realtime':
            self.execute("start transaction")
            try:
                try:
                    self.execute("select sql_id from sql_cache_tables where `table_name`=%r for update", table)
                except Exception, e:
                    pass
                ids = [row['sql_id'] for row in self]
                for id in ids:
                    self.execute("delete from sql_cache where id=%r", id)
                    self.execute("delete from sql_cache_tables where sql_id=%r", id)
                    self.execute("drop table if exists cache_%s", id)
            finally:
                self.end_transaction()

    def _calculate_set(self, **fields):
        """ Calculates the required set clause from the fields provided """
        tmp = []
        sql = []
        for k,v in fields.items():
            if k.startswith("__"):
                sql.append('`%s`=%b')
                k=k[2:]
            elif k.startswith("_"):
                sql.append('`%s`=%s')
                k=k[1:]
            else:
                sql.append('`%s`=%r')
                
            tmp.extend([k,v])
            
        return (','.join(sql), tmp)

    def update(self, table, where='1', _fast=False, **fields):
        sql , args = self._calculate_set(**fields)
        sql = "update %s set " + sql + " where %s "
        ## We are about to invalidate the table:
        if not _fast:
            self.invalidate(table)
        self.execute(sql, [table,] + args + [where,])

    def drop(self, table):
        self.invalidate(table)
        self.execute("drop table if exists `%s`", table)

    def delete(self, table, where='0', _fast=False):
        sql = "delete from %s where %s"
        ## We are about to invalidate the table:
        if not _fast:
            self.invalidate(table)
        self.execute(sql, (table, where))        

    def insert(self, table, _fast=False, **fields):
        """ A helper function to make inserting a little more
        readable. This is especially good for lots of fields.

        Special case: Normally fields are automatically escaped using
        %r, but if the field starts with _, it will be inserted using
        %s and _ removed.

        Note that since the introduction of the cached_execute
        functionality it is mandatory to use the insert, mass_insert
        or update methods to ensure the cache is properly invalidated
        rather than use raw SQL.
        """
        sql , args = self._calculate_set(**fields)
        sql = "insert into `%s` set " + sql
        ## We are about to invalidate the table:
        if not _fast:
            self.invalidate(table)
        self.execute(sql, [table,]+args)
                    
    def mass_insert_start(self, table, _fast=False):
        self.mass_insert_cache = {}
        self.mass_insert_table = table
        self.mass_insert_row_count = 0
        self.mass_insert_fast = _fast
    
    def mass_insert(self, args=None, **columns):
        """ Starts a mass insert operation. When done adding rows, call commit_mass_insert to finalise the insert.
        """
        if args: columns = args
        
        for k,v in columns.items():
            ## _field means to pass the field
            if k.startswith("__"):
                v=db_expand("%b", (v,))
                k=k[2:]
            elif k.startswith('_'):
                k=k[1:]
            else:
                v=db_expand("%r", (v,))
                
            try:
                self.mass_insert_cache[k][self.mass_insert_row_count]=v
            except:
                self.mass_insert_cache[k]={ self.mass_insert_row_count: v}

        self.mass_insert_row_count+=1
        if self.mass_insert_row_count > config.MASS_INSERT_THRESHOLD:
            self.mass_insert_commit()
            self.mass_insert_start(self.mass_insert_table, _fast=self.mass_insert_fast)

    def mass_insert_commit(self):
        try:
            keys = self.mass_insert_cache.keys()
        except AttributeError:
            ## We called commit without start
            return

        if len(keys)==0: return
        
        args = []
        values = []
        for i in range(self.mass_insert_row_count):
            for k in keys:
                try:
                    args.append(self.mass_insert_cache[k][i])
                except KeyError:
                    args.append('NULL')

            values.append(",".join(["%s"] * len(keys)))

        sql = "insert ignore into `%s` (%s) values (%s)" % (self.mass_insert_table,
                                                   ','.join(["`%s`" % c for c in keys]),
                                                   "),(".join(values))
        if not self.mass_insert_fast:
            self.invalidate(self.mass_insert_table)
            
        self.execute(sql,*args)

        ## Ensure the cache is now empty:
        self.mass_insert_start(self.mass_insert_table,
                               _fast=self.mass_insert_fast)

    def autoincrement(self):
        """ Returns the value of the last autoincremented key """
        return self.cursor.connection.insert_id()

    def next(self):
        """ The db object supports an iterator so that callers can simply iterate over the result set.

        Each iteration returns a hash as obtained from fetch. """
        result = self.fetch()
        if not result: raise StopIteration

        return result

    def fetch(self):
        """ Returns the next cursor row as a dictionary.

        It is encouraged to use this function over cursor.fetchone to ensure that if columns get reordered in the future code does not break. The result of this function is a dictionary with keys being the column names and values being the values """
        return self.cursor.fetchone()
    
    def check_index(self, table, key, idx_type='', length=None):
        """ This checks the database to ensure that the said table has an index on said key.

        If an index is missing, we create it here, so we always ensure an index exists once we return. """
        ## We implement a local cache to ensure that we dont hit the
        ## DB all the time:
        cache_key = "%s/%s" % (self.case,table)
        try:
            ## These should be the fields with the indexes on them:
            fields = DBIndex_Cache.get(cache_key)
        except KeyError:
            self.execute("show index from `%s`",table)
            fields = [ row['Key_name'] for row in self]
            DBIndex_Cache.put(fields, key=cache_key)

        ## Now fields is an array stored in the Store - we can append
        ## to it directly because we also hold a reference here and it
        ## will affect the next value gotten from the Store:
        if key not in fields:
            if length:
                sql="(`%s`(%s))" % (key,length)
            else:
                sql="(`%s`)" % (key) 

            pyflaglog.log(pyflaglog.VERBOSE_DEBUG,"Oops... No index found in table %s on field %s - Generating index, this may take a while" %(table,key))
            ## Index not found, we make it here:
            self.execute("Alter table `%s` add index %s %s",(table,idx_type,sql))

            ## Add to cache:
            fields.append(key)
        
    def get_meta(self, property, table='meta',**args):
        """ Returns the value for the given property in meta table selected database

        Only returns first value """
        self.execute("select value from `%s` where property=%r limit 1",
                     (table,property))
        row = self.fetch()

        return row and row.get('value')

    def set_meta(self, property,value, table='meta',force_create=False, **args):
        """ Sets the value in meta table
        """
        prevvalue = self.get_meta(property, table, **args)
        if (prevvalue != None) and (not force_create):
            self.execute("update `%s` set property=%r,value=%r where property=%r",
                         (table, property,value, property))
        else:
            self.execute("insert into `%s` set property=%r,value=%r", (table, property,value))

    def MakeSQLSafe(self,string):
        """ Returns a version of string, which is SQL safe.

        String will be converted to a form which is suitable to be used as the name of a table for example.
        """
        import re
        return re.sub('[^a-zA-Z0-9]','_',string)

    def get_temp(self):
        """ Gets a unique name for a table.

        This can be used to create temporary tables - since flag is multi-threaded, normal mysql temporary tables remain within the same thread. Use this function to get names for temporary tables which can be shared between all threads.

        Note that each DBO object maintains a list of temporary tables, and drops those when gc'd so users of this class do not need to clean temporary tables up.

        The result from this function is guaranteed to not exist - so a create temporary table (or even a create table) call should work.
        """
        thread_name = threading.currentThread().getName()
        thread_name = thread_name.replace('-','_')
        count = 1
        
        while 1:
            test_name = "%s_%s%s" % (thread_name, int(time.mktime(time.gmtime())),count)
            ## Check if the table already exists:
            self.execute('show table status like %r',test_name)
            rs = [ r for r in self ]
            if not rs:
                self.temp_tables.append(test_name)
                return test_name
            
            count+=1

    ## This flag controls if we shall discard our dbh from the
    ## pool. Normally dbhs are returned to the pool upon destructions
    ## but this can cause problems e.g. when we fork - so we have this
    ## as a way to stop the dbh from returning to the pool
    discard = False

    def __del__(self):
        """ Destructor that gets called when this object is gced """
        try:
            try:
                self.cursor.ignore_warnings = True
                for i in self.temp_tables:
                    self.drop(i)
            except: pass

            if self.transaction:
                self.end_transaction()

            ## Ensure that our mass insert case is comitted in case
            ## users forgot to flush it:
            self.mass_insert_commit()
            self.cursor.ignore_warnings = False

            ##key = "%s/%s" % (self.case, threading.currentThread().getName())
            key = "%s" % (self.case)
            if self.DBH and not self.discard:
                pool = self.DBH.get(key)
                pool.put(self.dbh)
                
        except (TypeError,AssertionError,AttributeError, KeyError),e:
            #print "dbh desctrucr: %s " % e
            pass
        except Exception,e:
            import pyflag.FlagFramework as FlagFramework
            
            print FlagFramework.get_bt_string(e)

class DirectDBO(PooledDBO):
    """ A class which just makes a new connection for each handle """
    dbh = None
    
    def get_dbh(self, case):
        try:
            self.dbh = mysql_connect(case)
        except Exception,e:
            ## We just failed to connect - i bet the cached variables
            ## are totally wrong - invalidate the cache:
            global mysql_connection_args
            
            mysql_connection_args = None            
            raise DBError("Unable to connects - does the DB Exist?: %s" % e)

    def __del__(self):
        if self.transaction:
            self.end_transaction()

        try:
            for i in self.temp_tables:
                self.drop(i)
        except: pass
        self.mass_insert_commit()
        if self.dbh:
            self.dbh.close()

config.add_option("DB_CONNECTION_TYPE", default="direct",
                  help="Type of database connections (direct, pooled)")

DBO = DirectDBO
if config.DB_CONNECTION_TYPE == 'pooled':
    print "You have selected pooled"
    DBO = PooledDBO

def escape_column_name(name):
    """ This is a handy utility to properly escape column names taking
    into account possible table names.
    """
    names = name.split(".")
    return '.'.join(["`%s`" % x for x in names])

def check_column_in_table(case, table, column_name, sql):
    """ This function checks that the column_name is defined in table
    and adds it if not. This is mostly used for schema migrations for
    later versions of pyflag.
    """
    dbh = DBO(case)
    try:
        dbh.execute("select `%s` from `%s` limit 1", column_name, table)
        row= dbh.fetch()
    except:
        dbh.execute("alter table `%s` add `%s` %s", table, column_name, sql)

## The following are utilitiy functions which can be used to manage
## schema changes
def convert_to_unicode(case, table):
    """ This function checks to see if the table is utf8 and if not we
    make it that. This is used to upgrade old tables in the pyflag db
    which happen to not be unicode. New tables should be automatically
    utf8.
    """
    try:
        dbh = DBO(case)
        dbh.execute("show create table %s", table)
        row = dbh.fetch()
        statement = row['Create Table'].splitlines()
        last_line = statement[-1]
        m = re.search("CHARSET=([^ ]+)",last_line)
        if m and m.group(1).lower() != "utf8":
            dbh.execute("alter table %s convert to charset 'utf8'", table)
    except:
        pass

if __name__=="__main__":
    config.set_usage(usage = "PyFlag Database Cache manager."
                     " (You must run this if you choose periodic caching).")

    config.add_option("period", default=60, type='int',
                      help = "Number of minutes to wait between cache refreshes")

    config.parse_options()

    while 1:
        pdbh = DBO()
        pdbh.execute("select value from meta where property='flag_db'")
        for row in pdbh:
            try:
                case = row['value']
                dbh = DBO(case)
                dbh2 = DBO(case)
                dbh.execute("select * from sql_cache")
                for row in dbh:
                    pyflaglog.log(pyflaglog.DEBUG, expand("Refreshing cache for %s %s: %s",
                                                          (case, row['timestamp'], row['query'])))
                    dbh2.insert("sql_cache",
                                query = row['query'],
                                status = 'progress',
                                limit = row['limit'],
                                length = row['length'],
                                _fast = True)
                    new_id = dbh2.autoincrement()
                    dbh2.execute("create table cache_%s %s limit %s,%s", (new_id, row['query'],
                                 row['limit'], row['length']))
                    dbh2.execute("update sql_cache set status='cached' where id=%r" , new_id)
                    dbh2.delete("sql_cache",
                                where = "id = %s" % row['id'])
                    dbh2.execute("commit")

                ## Now prune any cache tables
                dbh.execute("show tables")
                for row in dbh:
                    table = row.values()[0]
                    m=re.match("cache_(\d+)", table)
                    if m:
                        dbh2.execute("select * from sql_cache where id=%r", m.group(1))
                        if not dbh2.fetch():
                            dbh2.execute("drop table %s" % table)
                            pyflaglog.log(pyflaglog.DEBUG, "Dropping expired table %s" % table)
                    
            except Exception,e:
                pyflaglog.log(pyflaglog.ERROR, "Error: %s" % e)

        pyflaglog.log(pyflaglog.DEBUG, "Waiting for %s minutes" % config.PERIOD)
        time.sleep(config.PERIOD * 60)
