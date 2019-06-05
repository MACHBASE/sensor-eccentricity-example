# Sensor Eccentricity Example
This is client example codes for generating/analyzing sensor data of tire eccentricity, connected to MACHBASE. All you have to do is installing MACHBASE server, and configure your connection information into two .c files.

# Contents
* crt.sql: DDL queries for creating tables; measure_list and tag
* drop.sql: DDL queries for dropping tables, created in crt.sql
* sel.sql: SELECT queries
* measure.c: Source code for generating sample measure data and inserting into MACHBASE server
* retrieve.c: Source code for retrieving sample meagure data from MACHBASE
