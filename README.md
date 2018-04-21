### Make cryptdb Practical

Cryptdb originated from MIT. This is a modified version. In this project, we try to add new features, fix bugs we meet in our environment, and rewrite the code and comments to make the source code easy to understand. Introduction to the features will be posted at yiwenshao.github.io. Also, analysis of the source code will be posted there so that you do not need so much effort contributing to this project.

To deploy this version, you need follow the following steps.

+ have ubuntu 16.04 installed
+ install mysql-server 5.5 or higher, with the root password 'letmein'
	To verify this, you can use the command mysql -uroot -pletmein to log in
+ ./INSTALL.sh
+ source ~/.bashrc
+ source setup.sh
+ run ./cdbserver.sh
+ run ./cdbclient.sh 
+ enjoy it!



If you meet any problems installing it, or if you meet bugs or need new features that if not yet supported, post issues or contact me via shaoyiwenetATgmailDotcom.



New features added

+ Configurable Onions
+ foreign key constraint

```
create table student (id integer primary key);
create table choose (sid integer, foreign key fk(sid) references student(id));
insert into student values(1);
insert into choose values(1);

```

+ set user variable
+ timestamp
+ show create table
+ cdb_test for simple curd

obselete functions deleted

+ annotation
+ dbobject.tt

### features to be added

+ extended-insert
+ QUOTE
+ Search
