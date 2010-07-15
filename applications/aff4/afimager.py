import sys
try:
    import urwid
except ImportError:
    print "You must have urwid installed - try apt-get install python-urwid"
    sys.exit(-1)

import threading
import urwid.raw_display
import urwid.web_display
import urwid.curses_display
import time
import pdb
import os
from subprocess import PIPE, Popen

palette = [
    ('body','black','light gray', 'standout'),
    ('reverse','light gray','black'),
    ('header','white','dark red', 'bold'),
    ('important','dark blue','light gray',('standout','underline')),
    ('editfc','white', 'dark blue', 'bold'),
    ('editbx','light gray', 'dark cyan'),
    ('editcp','black','light gray', 'standout'),
    ('bright','dark gray','light gray', 'bold'),
    ('buttn','black','dark cyan'),
    ('buttnf','white','dark blue','bold'),
    ('foot','light gray', 'black'),
    ]


class FilenameEdit(urwid.Edit):
    """ An urwid edit box to implement filename completion """
    completion = None
    completion_pos = 0

    def keypress(self, size, key):
        try:
            if key == 'tab':
                path = self.get_edit_text()[:self.edit_pos]
                dirname = os.path.dirname(path)
                filename = os.path.basename(path)

                if not self.completion:
                    self.completion = [ x for x in os.listdir(dirname) if x.startswith(filename) ]
                    self.completion_pos = 0
                else:
                    self.completion_pos += 1
                    if self.completion_pos >= len(self.completion):
                        self.completion_pos = 0

                new_path = os.path.join(dirname, self.completion[self.completion_pos])
                self.set_edit_text(new_path)
                self.edit_pos = len(new_path)
            elif key == 'meta backspace':
                path = self.get_edit_text()[:self.edit_pos]
                if path[-1] == '/': path = path[:-1]
                new_path = os.path.dirname(path)
                if new_path[-1] != '/': new_path += '/'
                self.completion = None
                self.set_edit_text(new_path)
                self.edit_pos = len(new_path)
            else:
                self.completion = None
        except (IndexError, OSError): pass

        return self.__super.keypress(size, key)

class TextQuestion:
    def __init__(self, name, question, default = ''):
        self.question = question
        self.name = name
        self.default = default

    def widget(self):
        edit = urwid.Edit(('editcp', self.question), self.default)
        self.edit = urwid.AttrWrap(edit, 'editbx', 'editfc')

        return self.edit

    def value(self):
        return self.edit.get_edit_text()

class Filename(TextQuestion):
    def widget(self):
        edit = FilenameEdit(('editcp', self.question), self.default)
        self.edit = urwid.AttrWrap(edit, 'editbx', 'editfc')

        return self.edit

class Buttons:
    def __init__(self, name, buttons, default = None, actions = None):
        self.buttons = buttons
        self.name = name
        self.pressed = set()
        self.actions = actions
        self.default = default or buttons[0]

    def button_press(self, button, state=False):
        button = button.get_label()
        if button in self.pressed:
            self.pressed.remove(button)
        else:
            self.pressed.add(button)

        if self.actions and button in self.actions:
            self.actions[button]()

    def widget(self):
        return urwid.Padding(urwid.GridFlow(
                [urwid.AttrWrap(urwid.Button(txt, self.button_press),
                                'buttn','buttnf') for txt in self.buttons],
                13, 3, 1, 'left'),
                             ('fixed left',4), ('fixed right',3))

    def value(self):
        return self.pressed

class RadioButton(Buttons):
    def button_press(self, button, state=False):
        button = button.get_label()
        if state and self.actions and button in self.actions:
            self.actions[button](button)

    def widget(self):
        self.pressed = []
        result = urwid.Padding(urwid.GridFlow(
                [urwid.AttrWrap(urwid.RadioButton(self.pressed, txt, on_state_change = self.button_press),
                                'buttn','buttnf') for txt in self.buttons],
                13, 3, 1, 'left'),
                               ('fixed left',4), ('fixed right',3))

        if self.actions and self.default in self.actions:
            self.actions[self.default](self.default)

        return result

    def value(self):
        for x in self.pressed:
            if x.get_state():
                return x.get_label()

class Text:
    def __init__(self, name, default, style = None):
        self.name = name
        self.text = urwid.Text(default)

        if style:
            self.text = urwid.AttrWrap(self.text, style)

    def widget(self):
        return self.text

    def value(self):
        return self.text.get_text()

    def set_text(self, text):
        self.text.set_text(text)

class MyListBox(urwid.ListBox):
    def keypress(self, size, key):
        (maxcol, maxrow) = size

        focus_widget, pos = self.body.get_focus()
        if not focus_widget: return key

        if key in ['page up','page down']:
            if focus_widget.selectable():
                key = focus_widget.keypress((maxcol,),key)
            if key is None:
                self.make_cursor_visible((maxcol,maxrow))
                return

        return self.__super.keypress(size, key)

class ScrollableColumn(urwid.Columns):
    def __init__(self, widget_list, height = 10, **kwargs):
        self.scroll = urwid.Text("")
        self.scroll.selectable = lambda : False
        self.height = height
        widget_list = [
            ('fixed', 1, urwid.ListBox([urwid.AttrWrap(self.scroll, 'reverse')])),
            ] + widget_list

        kwargs['focus_column'] = 1
        self.__super.__init__(widget_list, **kwargs)

    def selectable(self):
        return True

    def keypress(self, size, key):
        self.scroll.set_text(key)
        if key == 'tab':
            return 'down'
        elif key in ['up', 'down', 'page up', 'page down']:
            return self.__super.keypress(size, key)
        else:
            return key

class Multiline(Text):
    def __init__(self, name, default, style = None, height = 10):
        self.name = name
        self.text = urwid.Text(default)
        self.height = height

    def widget(self):
        return urwid.BoxAdapter(
            urwid.Frame(
                ScrollableColumn(
                    [
                        urwid.ListBox([self.text]),
                    ], height = self.height
                )
            ), self.height)

class Form:
    text_header = ("Welcome to AFF4 Imager  "
        "UP / DOWN / PAGE UP / PAGE DOWN scroll.  Q exits.")
    footer_text = urwid.Text("")
    stop = False

    def __init__(self):

        Label = Text("explain", "Text", style = 'important')

        try:
            hdd_info = open("/proc/partitions").read()
        except: pass
        try:
            hdd_info += Popen(["ls", "-l", "/"], stdout=PIPE).communicate()[0]
        except: pass

        self.questions = [
            Multiline("info", default = hdd_info),
            Filename("input", "Input file location:"),
            Filename("output_volume", "Output Volume location:"),
            TextQuestion("stream_name", "Stream Name:"),
            Text("explain", "Select Imaging Mode", style = 'important'),
            RadioButton("mode", ['Simple','Sparse', 'Hash'], actions = dict(
                    Simple = lambda x: Label.set_text("Standard Default Imaging"),
                    Sparse = lambda x: Label.set_text("Sparse image - skip over unreadable regions"),
                    Hash   = lambda x: Label.set_text("Hash based image compression"),
                    )),
            Label,
            Buttons("buttons", ['Acquire', 'Quit'], actions = dict(Acquire = self.finish, Quit = self.finish)),
            ]

        self.build_gui()
        self.loop.run()

    def build_gui(self):
        blank = urwid.Divider()

        footer = urwid.AttrMap(self.footer_text, 'foot')

        listbox_content = []
        for x in self.questions:
            listbox_content.append(x.widget())
            listbox_content.append(blank)

        self.header = urwid.AttrWrap(urwid.Text(self.text_header), 'header')
        listbox = MyListBox(urwid.SimpleListWalker(listbox_content))

        frame = urwid.Frame(urwid.AttrWrap(listbox, 'body'),
                            header=self.header,
                            footer = footer)

        # use appropriate Screen class
        if urwid.web_display.is_web_request():
            self.screen = urwid.web_display.Screen()
        else:
            try:
                self.screen = urwid.raw_display.Screen()
            except:
                self.screen = urwid.curses_display.Screen()

        self.screen.register_palette(palette)

        self.loop = urwid.MainLoop(frame, palette, self.screen,
                                   unhandled_input=self.unhandled)

    def finish(self):
        self.stop = True
        raise urwid.ExitMainLoop()

    def get_results(self):
        result = {}
        for x in self.questions:
            result[x.name] = x.value()

        return result

    def unhandled(self, key):
        if key.lower() == 'q':
            self.header.set_text("Quitting  ....  Please Wait")
            self.loop.draw_screen()
            self.finish()
        elif key == 'r':
            self.loop.draw_screen()

        elif key == 'd':
            raise RuntimeError("Debug")

        if self.stop:
            raise urwid.ExitMainLoop()

class Main(threading.Thread, Form):
    progress = urwid.ProgressBar('black', 'header', 0, 100)
    textline = urwid.Text("hello world", align='left')
    textarea = [urwid.Text("A text")]
    stop = False

    def __init__(self):
        threading.Thread.__init__(self)

        blank = urwid.Divider()

        footer = urwid.AttrMap(self.footer_text, 'foot')

        listbox_content = [
            blank,
            self.textline,
            self.progress,
            urwid.Divider("-")
            ]

        self.header = urwid.AttrWrap(urwid.Text(self.text_header), 'header')
        listbox = urwid.ListBox(urwid.SimpleListWalker(listbox_content))
        self.text_area = urwid.AttrWrap(urwid.ListBox(self.textarea), 'editfc')
        header = urwid.AttrWrap(urwid.Text("Messages", align='center'), 'header')

        frame = urwid.Frame(urwid.AttrWrap(urwid.Pile([
                        ('fixed', 5, listbox),
                        ('flow', header),
                        self.text_area,
                        ], focus_item=2), 'body'),
                            header=self.header,
                            footer = footer)

        # use appropriate Screen class
        if urwid.web_display.is_web_request():
            self.screen = urwid.web_display.Screen()
        else:
            self.screen = urwid.raw_display.Screen()

        self.screen.register_palette(palette)

        self.loop = urwid.MainLoop(frame, palette, self.screen,
                                   unhandled_input=self.unhandled)

        ## Start up the worker thread
        self.start()

        ## This is the urwid event loop - its used to fetch keystrokes etc
        self.loop.run()

    def unhandled(self, key):
        if key.lower() == 'q':
            self.header.set_text("Quitting  ....  Please Wait")
            self.loop.draw_screen()
            self.finish()

        if self.stop:
            raise urwid.ExitMainLoop()

    def finish(self):
        self.stop = True
        self.join()

    def run(self):
        count = 10
        while not self.stop:
            time.sleep(0.15)

            widget, current_pos = self.text_area.get_focus()
            current_box_length = len(self.textarea) - 1
            self.textarea.append(urwid.Text("hello %s %s %s" % (count, current_pos, current_box_length)))

            ## Only scroll to the bottom if the scroll window is at the bottom
            if current_box_length == current_pos:
                self.text_area.set_focus(len(self.textarea))
                self.footer_text.set_text("")
            else: self.footer_text.set_text(" -----  More  ----")

            self.textline.set_text("%s" % count)
            self.progress.set_completion(count)

            self.loop.draw_screen()
            count += 1

            if count >= 100:
                self.stop = True
                self.footer_text.set_text("Imaging complete, press any key to exit ... ")
                self.loop.draw_screen()
                break

def setup():
    urwid.web_display.set_preferences("AFF4 Imager")
    # try to handle short web requests quickly
    if urwid.web_display.handle_short_request():
        return

    try:
        #Main()
        f = Form()
        results =  f.get_results()
        if 'Acquire' in results['buttons']: Main()
    except Exception, e:
        print e
        pdb.post_mortem()

if '__main__'==__name__ or urwid.web_display.is_web_request():
    setup()
