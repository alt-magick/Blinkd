# Blinkd
Command line tool to check for website updates

This tool should compile on windows and linux.

To compile on linux:<br>

g++ -o blinkd blinkd.cpp -std=c++17 -pthread
<br>

To use the program,<br>

Run:<br> 

blinkd add www.website.com<br> 
To create a list of websites to check.

Run: <br>

blinkd del www.website.com<br> 
To remove a website from the list.

Run blinkd without any arguments,<br> 
To list your websites ordered by the most recently updated.


