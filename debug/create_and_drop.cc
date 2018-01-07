#include "main/big_proxy.hh"
#include <vector>
using std::string;

std::vector<string> create{
"create database tpcc1000",
"use tpcc1000",
//"drop table if exists history;",
//"drop table if exists new_orders;",
//"drop table if exists orders;",
//"drop table if exists order_line;",
//"drop table if exists item;",
//"drop table if exists stock;",
"create table warehouse (w_id smallint unsigned, w_name varchar(10), w_street_1 varchar(20), w_street_2 varchar(20), w_city varchar(20), w_state char(2), w_zip char(9), w_tax integer unsigned, w_ytd integer unsigned) Engine=InnoDB;",
"create table district (d_id tinyint unsigned, d_w_id smallint unsigned, d_name varchar(10), d_street_1 varchar(20), d_street_2 varchar(20), d_city varchar(20), d_state char(2), d_zip char(9), d_tax integer unsigned, d_ytd integer unsigned, d_next_o_id int unsigned) Engine=InnoDB;",
"create table customer (c_id int unsigned, c_d_id tinyint unsigned,c_w_id smallint unsigned, c_first varchar(16), c_middle char(2), c_last varchar(16), c_street_1 varchar(20), c_street_2 varchar(20), c_city varchar(20), c_state char(2), c_zip char(9), c_phone char(16), c_since varchar(20), c_credit char(2), c_credit_lim bigint unsigned, c_discount bigint unsigned, c_balance bigint unsigned, c_ytd_payment bigint unsigned, c_payment_cnt smallint,c_delivery_cnt smallint, c_data VARCHAR(1000)) Engine=InnoDB;",
"create table history (h_c_id int, h_c_d_id tinyint, h_c_w_id smallint,h_d_id tinyint,h_w_id smallint,h_date varchar(20),h_amount integer unsigned, h_data varchar(24) ) Engine=InnoDB;",
"create table new_orders (no_o_id int not null,no_d_id tinyint not null,no_w_id smallint not null) Engine=InnoDB;",
"create table orders (o_id int not null, o_d_id tinyint not null, o_w_id smallint not null,o_c_id int,o_entry_d varchar(20),o_carrier_id tinyint,o_ol_cnt tinyint, o_all_local tinyint) Engine=InnoDB ;",
"create table order_line ( ol_o_id int not null, ol_d_id tinyint not null,ol_w_id smallint not null,ol_number tinyint not null,ol_i_id int, ol_supply_w_id smallint,ol_delivery_d varchar(20), ol_quantity tinyint, ol_amount integer unsigned, ol_dist_info char(24)) Engine=InnoDB ;",
"create table item (i_id int not null, i_im_id int, i_name varchar(24), i_price integer unsigned, i_data varchar(50)) Engine=InnoDB;",
"create table stock (s_i_id int not null, s_w_id smallint not null, s_quantity smallint, s_dist_01 char(24), s_dist_02 char(24),s_dist_03 char(24),s_dist_04 char(24), s_dist_05 char(24), s_dist_06 char(24), s_dist_07 char(24), s_dist_08 char(24), s_dist_09 char(24), s_dist_10 char(24), s_ytd integer unsigned,s_order_cnt smallint, s_remote_cnt smallint,s_data varchar(50)) Engine=InnoDB;",
"SHOW TABLES;"
};


std::vector<string> drop{
"drop database tpcc1000;"
};


int
main(int argc,char ** argv) {
    if(argc!=2){
        std::cout<<"expect 1 argument"<<std::endl;
        return 0;
    }
    string option(argv[1]);
    big_proxy b;
    if(option == string("create")){
        for(auto item:create){
            b.go(item);
        }
    }else if(option == string("drop")){
        for(auto item:drop){
            b.go(item);
        }
    }else{
        std::cout<<"unknow option"<<std::endl;
    }
    return 0;
}

