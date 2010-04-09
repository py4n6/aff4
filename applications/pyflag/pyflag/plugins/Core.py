""" Plugin to create core services """
import Framework
import pyaff4
from PyFlagConstants import *
import pdb
import sqlite3
import os.path
import conf
config=conf.ConfObject()

oracle = Framework.oracle

## Constants related to Columns
## The dataType of the target attribute
ATTRIBUTE_DATATYPE = PYFLAG_NAMESPACE + "column/dataType"
ATTRIBUTE_NAME =  PYFLAG_NAMESPACE + "attribute/name"
COLUMN_NAME     = PYFLAG_NAMESPACE + "column/name"
COLUMN_SQL_TYPE = PYFLAG_NAMESPACE + "column/sql_type"
COLUMN_SQL_NAME = PYFLAG_NAMESPACE + "column/sql_name"

## Register important PyFlag RDF Objects.

## Tables are a collection of columns.  Columns are special classes
## which abstract conversion from RDF to SQL. When we create a new
## volume we write the schema on it in a special graph. This just
## shows how we lay out RDF data. By saving the schema into the volume
## we make the volume totally self dependent so even future versions
## of PyFlag will be able to open older volumes in exactly the same
## way.
class Column(pyaff4.AFFObject):
    """ A table column is specifically designed to build a table """
    dataType = "pyflag://Column"
    name = ''
    sql_name = ''
    sql_type = ''
    attribute_name = ''
    rdfvalue_class = pyaff4.XSDString

    def __init__(self, urn=None, mode='w', *args):
        pyaff4.AFFObject.__init__(self, urn, mode)
        #self.proxied = pyaff4.AFFObject(urn, mode)
        if urn:
            if mode == 'w':
                print "Creating new column %s" % urn.value
                Framework.set_metadata(urn, RDF_TYPE, self.dataType, graph='schema',
                                       rdftype=pyaff4.RDFURN)
            if mode == 'r':
                self.value = self.rdfvalue_class()
                print "Loading column %s" % urn.value
                self.name = oracle.resolve_alloc(urn, COLUMN_NAME).value
                self.sql_type = oracle.resolve_alloc(urn, COLUMN_SQL_TYPE).value
                self.sql_name = oracle.resolve_alloc(urn, COLUMN_SQL_NAME).value
                self.attribute_name = oracle.resolve_alloc(urn, ATTRIBUTE_NAME).value

    def sql_create(self):
        return '"%s" %s' % (self.sql_name, self.sql_type)

    def close(self):
        if self.mode == 'w':
            Framework.set_metadata(self.urn, COLUMN_NAME, self.name, graph='schema')
            Framework.set_metadata(self.urn, ATTRIBUTE_NAME,
                                   self.attribute_name, graph='schema',
                                   rdftype = pyaff4.RDFURN)
            Framework.set_metadata(self.urn, COLUMN_SQL_TYPE, self.sql_type, graph='schema')
            Framework.set_metadata(self.urn, COLUMN_SQL_NAME, self.sql_name, graph='schema')

    def finish(self):
        self.__init__(self.urn, self.mode)
        ## NOTE: We must wrap the returned object with a
        ## ProxiedAFFObject so we continue to receive calls to it.
        return pyaff4.ProxiedAFFObject(self)

    def add_value(self, urn):
        """ Try to resolve the attribute of this urn and emit SQL that
        would insert it into the table.
        """
        if not oracle.resolve_value(urn, self.attribute_name, self.value):
            raise KeyError("%s not found" % self.attribute_name)

        return self.value.value

class URLColumn(Column):
    dataType = HTTP_URL
    name = 'URL'
    sql_name = 'url'
    sql_type = 'VARCHAR(1024)'
    attribute_name = HTTP_URL
    rdfvalue_class = pyaff4.RDFURN

class DateColumn(Column):
    dataType = TIMESTAMP_CREATED
    name = "Timestamp"
    sql_name = 'timestamp'
    sql_type = 'TIMESTAMP'
    attribute_name = pyaff4.AFF4_TIMESTAMP
    rdfvalue_class = pyaff4.XSDDatetime

class Table(pyaff4.AFFObject):
    """ A Table is a collection of columns """
    dataType = PYFLAG_TABLE

    def make_table_filename(self, urn):
        ## Make a filename based on the urn to use for storing the
        ## sqlite db in. For now we just use a name derived from the
        ## URN.
        name = urn.parser.query.replace("/","_")
        return name

    def __init__(self, urn=None, mode='w', *args):
        pyaff4.AFFObject.__init__(self, urn, mode)
        if urn:
            self.columns = []

            if mode == 'w':
                print "Creating new table %s" % urn.value
                Framework.set_metadata(urn, RDF_TYPE, self.dataType, graph='schema',
                                       rdftype=pyaff4.RDFURN)
                name = self.make_table_filename(urn)
                Framework.set_metadata(urn, pyaff4.AFF4_TARGET, name,
                                       graph = 'schema')
            if mode == 'r':
                print "Loading table %s" % urn.value

                ## Read all the columns
                iter = oracle.get_iter(urn, PYFLAG_COLUMN)
                while 1:
                    res = oracle.alloc_from_iter(iter)
                    if not res: break

                    self.columns.append(res)

                ## Get the target file
                name = oracle.resolve_alloc(urn, pyaff4.AFF4_TARGET).value

            self.filename = os.path.join(config.RESULTDIR, "%s.sqlite" % name)

    def close(self):
        if self.mode == 'w':
            for column in self.columns:
                if type(column)==str:
                    urn = pyaff4.RDFURN()
                    urn.set(Framework.RESULT_VOLUME.volume_urn.value)
                    urn.add(column)
                else:
                    urn = column

                Framework.set_metadata(self.urn, PYFLAG_COLUMN, urn, graph='schema')

                ## Create the sqlite file
                self.create_file()

    def create_file(self):
        self.con = sqlite3.connect(self.filename)
        print "Creating new database %s" % self.filename
        columns = []

        cursor = self.con.cursor()
        for column in self.columns:
            pdb.set_trace()

            c = oracle.open(column, 'r')
            try:
                columns.append(c.sql_create())
                print column, column.value, c.sql_create()

            finally:  c.cache_return()

        cmd = 'CREATE  TABLE  IF NOT EXISTS "http" ("id" INTEGER NOT NULL , %s)' % ",".join(columns)
        print cmd
        cursor.execute(cmd)


    def finish(self):
        self.__init__(self.urn, self.mode)
        ## NOTE: We must wrap the returned object with a
        ## ProxiedAFFObject so we continue to receive calls to it.
        return pyaff4.ProxiedAFFObject(self)

    def add_object(self, urn):
        row = []
        ## Iterate over all our columns and ask them to produce SQL to
        ## insert the data into the db.
        for column in self.columns:
            c = Framework.oracle.open(column, 'r')
            try:
                row.append(c.add_value(urn))
            except KeyError: pass
            finally: c.cache_return()

        if row:
            print row

class CoreEvents(Framework.EventHandler):
    """ Core events to set up PyFlag """
    def startup(self):
        print "Installing columns"
        self.register_schema(URLColumn)
        self.register_schema(DateColumn)
        self.register_schema(Table)

        print "Installing new volume"
        Framework.RESULT_VOLUME = Framework.ResultVolume()
        Framework.set_metadata = Framework.RESULT_VOLUME.set_metadata

    def register_schema(self, class_ref):
        Framework.oracle.register_type_dispatcher(
            class_ref.dataType, pyaff4.ProxiedAFFObject(class_ref))

    def create_volume(self):
        ## Build up the schema
        columns = [
            Framework.RESULT_VOLUME.add_column(URLColumn, "column/http/url"),
            Framework.RESULT_VOLUME.add_column(DateColumn, "column/timestamp/create"),
            ]

        Framework.RESULT_VOLUME.add_table("table/http", columns)
