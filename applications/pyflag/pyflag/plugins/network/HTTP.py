import Scanner
import re, os, os.path
import pyaff4
import PCAP
import pdb
import gzip
import Framework
from PyFlagConstants import *

oracle = pyaff4.Resolver()

class RelaxedGzip(gzip.GzipFile):
    """ A variant of gzip which is more relaxed about errors """
    def _read_eof(self):
        """ Dont check crc """
        pass

    def _read(self, size=1024):
        """ Trap possible decompression errors """
        try:
            ## just do big reads here
            return gzip.GzipFile._read(self, 1024000)
        except (EOFError, IOError): raise

        ## Swallow exceptions
        except Exception,e:
            return ''

class HTTPScanner(Scanner.BaseScanner):
    """ Scan for HTTP connections """
    request_re = re.compile("^(GET|POST|PUT|OPTIONS|PROPFIND) +([^ ]+) +HTTP/1\..",
                                     re.IGNORECASE)
    response_re = re.compile("HTTP/1\.. (\\d+) +", re.IGNORECASE)

    def scan(self, buffer, fd, scanners):
        if self.request_re.search(buffer):
            self.forward_fd = fd
            ## Find the reverse fd
            base = os.path.dirname(fd.urn.value)

            urn = pyaff4.RDFURN()
            urn.set(base)
            urn.add("forward.pkt")
            self.forward_pkt_fd = oracle.open(urn, 'r')

            urn.set(base)
            urn.add("reverse")
            self.reverse_fd = oracle.open(urn, 'r')

            urn.set(base)
            urn.add("reverse.pkt")
            self.reverse_pkt_fd = oracle.open(urn, 'r')

            try:
                self.process(scanners)
            finally:
                self.forward_pkt_fd.cache_return()
                self.reverse_fd.cache_return()
                self.reverse_pkt_fd.cache_return()

    def read_headers(self, request, forward_fd):
        while True:
            line = forward_fd.readline()
            if not line or line=='\r\n':
                return True

            tmp = line.split(':',1)
            try:
                request[tmp[0].lower().strip()] =tmp[1].strip()
            except IndexError:
                pass

    def read_request(self, request, forward_fd):
        """ Checks if line looks like a URL request. If it is, we
        continue reading the fd until we finish consuming the request
        (headers including post content if its there).

        We should be positioned at the start of the response after this.
        """
        line = forward_fd.readline()
        m=self.request_re.search(line)
        if not m: return False

        request['method']=m.group(1)

        self.read_headers(request, forward_fd)

        try:
            host = request['host']
        except KeyError:
            dest_ip = forward_fd.get_metadata(DEST_IP)
            host = dest_ip.serialise(None)

        url = pyaff4.RDFURN()
        url.set("http://%s/%s" % (host, m.group(2)))

        request['url'] = url
        request['host'] = host

        return True

    def skip_body(self, headers, stream_fd):
        """ Reads the body of the HTTP object depending on the values
        in the headers. This function takes care of correctly parsing
        chunked encoding. We return a new map representing the HTTP
        object.

        We assume that the fd is already positioned at the very start
        of the object. After this function we will be positioned at
        the end of this object.

        We return a new URN which can be opened for reading the body.
        """
        http_object_urn = self.handle_encoding(headers, stream_fd)
        ## Handle gzip encoded data
        if headers.get('content-encoding') == 'gzip':
            ## Close the old file - we need to make a new one now instead
            http_object = oracle.open(http_object_urn, 'w')
            http_object.close()

            fd = oracle.open(http_object_urn, 'r')
            try:
                gzip_fd = RelaxedGzip(fileobj = fd, mode='rb')
                http_object = Framework.PyFlagStream("decompressed", base = fd.urn.value)
                try:
                    while 1:
                        data = gzip_fd.read(1024*1024)
                        if not data: break

                        http_object.write(data)

                    return http_object.urn
                ## Close the new object and return its URN. It is now
                ## ready for reading:
                finally:
                    http_object.cache_return()

            finally:
                fd.close()

        return http_object_urn

    def handle_encoding(self, headers, fd):
        if fd.readptr == 205:
            pdb.set_trace()
        http_object = Framework.PyFlagMap("HTTP/%s" % fd.readptr,
                                          base = fd.urn.value)
        try:
            try:
                skip = int(headers['content-length'])
                http_object.write_from(fd.urn, fd.tell(), skip)
                fd.seek(skip, 1)

                return http_object.urn
            except KeyError:
                pass

            ## If no content-length is specified maybe its chunked
            try:
                if "chunked" in headers['transfer-encoding'].lower():
                    while True:
                        line = fd.readline()
                        try:
                            length = int(line,16)
                        except:
                            return http_object.urn

                        if length == 0:
                            return http_object.urn

                        ## There is a \r\n delimiter after the data chunk
                        http_object.write_from(fd.urn, fd.tell(), length)
                        fd.seek(length+2,1)
                    return http_object.urn
            except KeyError:
                pass

            ## If the header says close then the rest of the file is the
            ## body (all data until connection is closed)
            try:
                if "close" in headers['connection'].lower():
                    http_object.write_from(fd.urn, fd.tell(), fd.size.value - fd.tell())
            except KeyError:
                pass

            return http_object.urn
        finally:
            http_object.cache_return()

    def process_cookies(self, request, target):
        """ Merge in cookies if possible """
        try:
            cookie = request['cookie']
            C = Cookie.SimpleCookie()
            C.load(cookie)
            for k in C.keys():
                target.insert_to_table('http_parameters',
                                       dict(key = k,
                                            value = C[k].value))
                target.dirty = 1
        except (KeyError, Cookie.CookieError):
            pass

    def process_parameter(self, key, value, target):
        """ All parameters are attached to target """
        ## Non printable keys are probably not keys at all.
        if re.match("[^a-z0-9A-Z_]+",key): return

        ## Deal with potentially very large uploads:
        if hasattr(value,'filename') and value.filename:
            new_urn = target.urn.parser.query + "/" + value.filename

            ## dump the file to the AFF4 volume
            fd = CacheManager.AFF4_MANAGER.create_cache_fd(target.case, new_urn,
                                                           inherited = target.urn)
            ## More efficient copying:
            while 1:
                data = value.file.read(1024*1024)
                if not data: break

                fd.write(data)

            ## We store the urn of the dumped file in place of the value
            value = fd.urn
            fd.close()
        else:
            try:
                value = value.value
            except: pass

        target.insert_to_table('http_parameters',
                               dict(key = key,
                                    value = value))
        target.dirty = 1

    def process_post_body(self, request, request_body, target):
        try:
            base, query = request['url'].split('?',1)
        except ValueError:
            base = request['url']
            query = ''
        except KeyError:
            return

        request_body.seek(0)
        env = dict(REQUEST_METHOD=request['method'],
                   CONTENT_TYPE=request.get('content-type',''),
                   CONTENT_LENGTH=request_body.size.value,
                   REQUEST_BODY=request_body.urn.value,
                   QUERY_STRING=query)

        result = cgi.FieldStorage(environ = env, fp = request_body)
        self.count = 1

        if type(result.value) == str:
            result = {str(result.name): result}

        for key in result:
            self.process_parameter(key, result[key], target)

        self.process_parameter("request_urn", request_body.urn, target)

    def read_response(self, response, fd):
        """ Checks if line looks like a HTTP Response. If it is, we
        continue reading the fd until we finish consuming the
        response.

        We should be positioned at the start of the next request after this.
        """
        ## Search for something that looks like a response:
        while 1:
            line = fd.readline()
            if not line: return False

            m=self.response_re.search(line)
            if m: break

        response['HTTP_code']= m.group(1)
        self.read_headers(response, fd)

        return True

    def process(self, scanners):
        print "Opening %s as a HTTP Request" % self.forward_fd.urn.parser.query
        while True:
            parse = False
            request_body = response_body = None
            request = {}
            response = {}

            ## First parse both request and response
            ## Get the current timestamp of the request
            packet = PCAP.dissect_packet(self.forward_fd, self.forward_pkt_fd)

            if self.read_request(request, self.forward_fd):
                try:
                    request['timestamp'] = packet.ts_sec
                except AttributeError:
                    request['timestamp'] = 0

                parse = True
                request_body_urn = self.skip_body(request, self.forward_fd)

                try: Framework.set_metadata(request_body_urn, HTTP_URL, request['url'])
                except KeyError: pass

                try: Framework.set_metadata(request_body_urn, HTTP_METHOD, request['method'])
                except KeyError: pass

                try: Framework.set_metadata(request_body_urn, HTTP_CODE, response['HTTP_code'])
                except KeyError: pass

                try: Framework.set_metadata(request_body_urn, HOST_TLD,
                                            Framework.make_tld(request['host']))
                except KeyError: pass

                ## Finalise the object
                request_body = oracle.open(request_body_urn, 'w')
                request_body.close()

            packet = PCAP.dissect_packet(self.reverse_fd, self.reverse_pkt_fd)
            if self.read_response(response, self.reverse_fd):
                try:
                    response['timestamp'] = packet.ts_sec
                except AttributeError:
                    response['timestamp'] = 0

                parse = True
                response_body_urn = self.skip_body(response, self.reverse_fd)

                response_body = oracle.open(response_body_urn, 'w')
                response_body.close()

            return
            ## We hang all the parameters on the response object
            ## (i.e. file attachment, post parameters, cookies)
            if response_body and request_body:
                self.process_cookies(request, response_body)
                self.process_post_body(request, request_body, response_body)
                if request_body.size > 0:
                    request_body.close()

            if response_body and response_body.size > 0:
                ## Store information about the object in the http table:
                url = request.get('url','/')

                ## We try to store the url in a normalized form so we
                ## can find it regardless of the various permutations
                ## it can go through
                response_body.insert_to_table("http",
                                              dict(method = request.get('method'),
                                                   url = url,
                                                   status = response.get('HTTP_code'),
                                                   content_type = response.get('content-type'),
                                                   useragent = request.get('user-agent'),
                                                   host = request.get('host'),
                                                   tld = make_tld(request.get('host',''))
                                                   )
                                              )
                response_body.close()
                Scanner.scan_inode_distributed(forward_fd.case, response_body.inode_id,
                                               scanners, self.cookie)

