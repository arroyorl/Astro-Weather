@echo off

cd %homedrive%%homepath%\downloads

if %1.==. goto :nourl
set server=%1

set timeout=30
if %2.==. goto :notimeout
set timeout=%2
:notimeout

rem filename
set file=%homedrive%%homepath%\downloads\boltwood.txt
if %3.==. goto :nofile
set file=%3
:nofile

:repeat
	curl %server% > %file%
	timeout %timeout%
goto :repeat


:nourl
@echo usage: boltwood.bat server_url [timeout  filename]
@echo		default filename: %homedrive%%homepath%\downloads\boltwood.txt
@echo		defailt time: 30 secs.
