# Add more folders to ship with the application, here
folder_01.source = qml/QLocalChat
folder_01.target = qml
DEPLOYMENTFOLDERS = folder_01

# Additional import path used to resolve QML modules in Creator's code model
QML_IMPORT_PATH =

symbian:TARGET.UID3 = 0xEA47B83C

QT += network
QT += xmlpatterns

# Allow network access on Symbian
symbian:TARGET.CAPABILITY += NetworkServices

# The .cpp file which was generated for your project. Feel free to hack it.
SOURCES += main.cpp

