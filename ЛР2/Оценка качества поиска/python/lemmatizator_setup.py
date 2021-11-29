# -*- coding: utf-8 -*-

import PyInstaller.__main__
from distutils.dir_util import copy_tree
import sys
from os import path

dir = path.dirname(sys.executable)

path2packages = path.join(dir, "Lib", "site-packages")

packages = ["natasha", "pymorphy2", "pymorphy2-0.9.1.dist-info",
            "pymorphy2_dicts_ru", "pymorphy2_dicts_ru-2.4.417127.4579844.dist-info"]

for package in packages:
    if not path.exists(path.join(path2packages, package)):
        print("Error: package {:s} not exists".format(package))
        exit(1)

PyInstaller.__main__.run([
    "--hidden-import",  '"natasha"',
    "--noconfirm",
    "lemmatizator.py"
])

for package in packages:
    copy_tree(path.join(path2packages, package), path.join("dist\\lemmatizator", package))