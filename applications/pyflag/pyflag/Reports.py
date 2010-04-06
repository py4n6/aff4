class ReportError(Exception):
    ui = None
    pass

class Report:
    hidden = False
    order = 10
    parameters = []

    """ Baseclass for report implementation """
    def display(self, query, result):
        pass


    def form(self, query, result):
        pass

    def check_parameters(self, query):
        for parameter in self.parameters:
            if not query.has_key(parameter):
                return False

        return True
