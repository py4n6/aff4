""" Plugin to create core services """
import Framework
import pyaff4
from PyFlagConstants import *
import pdb

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
class Column(Framework.Proxy):
    """ A table column is specifically designed to build a table """
    dataType = "pyflag://Column"
    name = ''
    sql_name = ''
    sql_type = ''
    attribute_dataType = ''
    attribute_name = ''

    def __init__(self, urn=None, mode='w', *args):
        self.proxied = pyaff4.AFFObject(urn, mode)
        if urn:
            if mode == 'w':
                print "Creating new column %s" % urn.value
                Framework.set_metadata(urn, RDF_TYPE, self.dataType, graph='schema',
                                       rdftype=pyaff4.RDFURN)
            if mode == 'r':
                print "Loading column %s" % urn.value
                self.attribute_dataType = Framework.oracle.resolve_alloc(
                    urn, ATTRIBUTE_DATATYPE)
                self.name = Framework.oracle.resolve_alloc(urn, COLUMN_NAME).value
                self.sql_type = Framework.oracle.resolve_alloc(urn, COLUMN_SQL_TYPE).value
                self.sql_name = Framework.oracle.resolve_alloc(urn, COLUMN_SQL_NAME).value
                self.attribute_name = Framework.oracle.resolve_alloc(urn, ATTRIBUTE_NAME).value

    def close(self):
        if self.mode == 'w':
            Framework.set_metadata(self.urn, COLUMN_NAME, self.name, graph='schema')
            Framework.set_metadata(self.urn, ATTRIBUTE_DATATYPE,
                                   self.attribute_dataType, graph='schema',
                                   rdftype = pyaff4.RDFURN)
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

class URLColumn(Column):
    dataType = HTTP_URL
    name = 'URL'
    sql_name = 'url'
    sql_type = 'VARCHAR(1024)'
    attribute_dataType = pyaff4.DATATYPE_RDF_URN
    attribute_name = HTTP_URL

class DateColumn(Column):
    dataType = TIMESTAMP_CREATED
    name = "Timestamp"
    sql_name = 'timestamp'
    sql_type = 'TIMESTAMP'
    attribute_dataType = pyaff4.DATATYPE_XSD_DATETIME
    attribute_name = pyaff4.AFF4_TIMESTAMP

class Table(Framework.Proxy):
    """ A Table is a collection of columns """
    dataType = PYFLAG_TABLE
    columns = []

    def __init__(self, urn=None, mode='w', *args):
        self.proxied = pyaff4.AFFObject(urn, mode)
        if urn:
            if mode == 'w':
                print "Creating new table %s" % urn.value
                Framework.set_metadata(urn, RDF_TYPE, self.dataType, graph='schema',
                                       rdftype=pyaff4.RDFURN)
            if mode == 'r':
                print "Loading table %s" % urn.value
                iter = oracle.get_iter(urn, PYFLAG_COLUMN)
                while 1:
                    res = oracle.alloc_from_iter(iter)
                    if not res: break

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

    def finish(self):
        self.__init__(self.urn, self.mode)
        ## NOTE: We must wrap the returned object with a
        ## ProxiedAFFObject so we continue to receive calls to it.
        return pyaff4.ProxiedAFFObject(self)

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
