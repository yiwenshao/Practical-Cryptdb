#include <my_global.h>
#include <mysql.h>
void get_version(){
    printf("MySQL client version: %s\n", mysql_get_client_info());
}
void finish_with_error(MYSQL *con){
  fprintf(stderr, "%s\n", mysql_error(con));
  return;        
}
MYSQL_RES * queryResults(MYSQL *con,char* query){
    if (mysql_query(con, query)) {
        finish_with_error(con);
    }
    MYSQL_RES *result = mysql_store_result(con);
    if (result == NULL) {
        printf("query no results");
    }
    return result;   
}
MYSQL * getConnection(char *ip,char *user,char *password, char *db){
    MYSQL *con = mysql_init(NULL);
    if (con == NULL){
        fprintf(stderr, "mysql_init() failed\n");
        exit(1);
    }
    if (mysql_real_connect(con, ip, user, password,
            db, 0, NULL, 0) == NULL) {
        finish_with_error(con);
    }
    return con;
}
void *showResults(MYSQL *con,MYSQL_RES *result){
    int num_fields = mysql_num_fields(result);
    MYSQL_ROW row;
    char *esp = (char*)malloc(sizeof(char)*27);
    while ((row = mysql_fetch_row(result))) {
        //print escaped string here
        unsigned long * fieldLen = mysql_fetch_lengths(result);
        for(int i = 0; i < num_fields; i++) {
            mysql_real_escape_string(con,esp,row[i],fieldLen[i]);
            printf("%s ", esp? esp : "NULL"); 
        } 
            printf("\n"); 
    }
}
int main(int argc, char **argv){
    get_version();
    MYSQL *con = getConnection("localhost","root","letmein","tdb");
    MYSQL_RES *result = queryResults(con,"select * from student");
    showResults(con,result);
    mysql_free_result(result);
    mysql_close(con);
    return 0;
}
