# A basic GUI program using TK
# Created by David Briddock

from Tkinter import *
import tkMessageBox

# initialise main window
def init(win):
  win.title("Basic GUI Program")
  win.minsize(200, 50)
  btn.pack()

# find button callback
def hello():
  tkMessageBox.showinfo("Hello", "The callback worked!")

# create top-level window object
win = Tk()

# create a widget
btn = Button(win, text="Hello", command=hello)

# initialise and start main loop
init(win)
mainloop()

