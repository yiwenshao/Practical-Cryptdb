cd ../..
find . | grep '\.cc$\|\.c$\|\.h$\|\.hh$' | xargs ctags
rm cscope*
find . | grep '\.cc$\|\.c$\|\.h$\|\.hh$' > cscope.files
cscope -R -b -i cscope.files
