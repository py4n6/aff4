########################################################
# Template system
########################################################
class Wrapper:
    def __init__(self, dict):
        self.dict = dict
    def __getitem__(self, item):
        raise NotImplementedError

class Eval (Wrapper):
    def __getitem__(self, item):
        return eval (item, self.dict)
