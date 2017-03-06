COLOR_END = '\027[00m'

function redtext(x)
    return '\027[1;31m' .. x .. COLOR_END
end

function greentext(x)
    return '\027[1;92m'.. x .. COLOR_END
end

function orangetext(x)
    return '\027[01;33m'.. x .. COLOR_END
end

g=1

queryType = {}
queryType[proxy.COM_SLEEP] = "COM_SLEEP"
queryType[proxy.COM_QUIT] = "COM_QUIT"
queryType[proxy.COM_INIT_DB] = "COM_INIT_DB"
queryType[proxy.COM_QUERY] = "COM_QUERY"
queryType[proxy.COM_FIELD_LIST]= "COM_FIELD_LIST"
queryType[proxy.COM_CREATE_DB]= "COM_CREATE_DB"
queryType[proxy.COM_DROP_DB]= "COM_DROP_DB"
queryType[proxy.COM_REFRESH]= "COM_REFRESH"
queryType[proxy.COM_SHUTDOWN] = "COM_SHUTDOWN"
queryType[proxy.COM_STATISTICS] = "COM_STATISTICS"
queryType[proxy.COM_PROCESS_INFO] = "COM_PROCESS_INFO"
queryType[proxy.COM_CONNECT] = "COM_CONNECT"
queryType[proxy.COM_PROCESS_KILL] = "COM_PROCESS_KILL"
queryType[proxy.COM_DEBUG] = "COM_DEBUG"
queryType[proxy.COM_PING] = "COM_PING"
queryType[proxy.COM_TIME] = "COM_TIME"
queryType[proxy.COM_DELAYED_INSERT] = "COM_DELAYED_INSERT"
queryType[proxy.COM_CHANGE_USER] = "COM_CHANGE_USER"
queryType[proxy.COM_BINLOG_DUMP] = "COM_BINLOG_DUMP"
queryType[proxy.COM_TABLE_DUMP] = "COM_TABLE_DUMP"
queryType[proxy.COM_CONNECT_OUT] = "COM_CONNECT_OUT"
queryType[proxy.COM_REGISTER_SLAVE] = "COM_REGISTER_SLAVE"
queryType[proxy.COM_STMT_PREPARE] = "COM_STMT_PREPARE"
queryType[proxy.COM_STMT_EXECUTE] = "COM_STMT_EXECUTE"
queryType[proxy.COM_STMT_SEND_LONG_DATA] = "COM_STMT_SEND_LONG_DATA"
queryType[proxy.COM_STMT_CLOSE] = "COM_STMT_CLOSE"
queryType[proxy.COM_STMT_RESET] = "COM_STMT_RESET"
queryType[proxy.COM_SET_OPTION] = "COM_SET_OPTION"
queryType[proxy.COM_STMT_FETCH] = "COM_STMT_FETCH"
queryType[proxy.COM_DAEMON] = "COM_DAEMON"






function printCS()
    server = nil
    client = nil
    sp = nil
    if proxy.connection.client ~= nil then
        client = proxy.connection.client.src.name
    end
    
    if proxy.connection.server ~= nil then
        server = proxy.connection.server.dst.address
        sp = proxy.connection.server.dst.port
    end
    if client~= nil then
        print(redtext("clientName:"..client))
    else
        print(redtext("clientName=nil"))
    end

    if server ~= nil then
        print(redtext(server))
    else 
        print(redtext("server=nil"))
    end
    if sp ~= nil then
        print(redtext(sp))
    else
        print(redtext("sp = nil"))
    end
end


function connect_server()
    print(orangetext("connect_server"))
    printCS()
    if g == 1 then 
        g = 0
    else g = 1
    end
    print("g "..g)
    print("ndx "..proxy.connection.backend_ndx.."get: "..#proxy.global.backends)

end

function read_handshake()
    print(orangetext("read_handshake"))
    printCS()
end

function read_auth()
    print(orangetext("read_auth"))
    printCS()
end

function read_auth_result()
    print(orangetext("read_auth_result"))
    printCS()
end

function read_query( packet )
    print(orangetext("read_query"))
    print(redtext("query type: "..queryType[string.byte(packet)]))
    printCS()
    
    if string.byte(packet) == proxy.COM_QUERY then
	print("we got a normal query: " .. string.sub(packet, 2))
        proxy.queries:append(1, packet, {resultset_is_needed = true})
        return proxy.PROXY_SEND_QUERY
   else
 	print("we got a abnormal query: " .. string.sub(packet, 2))
    end
end


function print_fields(infields)
    local resfields = infields
    local interim_fields = {}
    --store fileds in interim_fields
    if (#resfields) then
           io.write("|")
    end
    for i = 1, #resfields do
        rfi = resfields[i]
        interim_fields[i] ={ type = resfields[i].type,name = resfields[i].name }
        io.write(string.format("%-20s|",rfi.name))
    end
end


function print_rows(inrows)
    local resrows = inrows
    local interim_rows = {}
    for row in resrows do
        table.insert(interim_rows, row)
        io.write("|")
--        for key,value in pairs(row) do
--            io.write(string.format("%-20s|", value))
--        end
         for k,v in pairs(row) do
               if v ~= nil then
                   io.write(string.format("%-20s|", v))
		   io.write("size = "..string.len(v))
               else
                   io.write(string.format("%-20s|", "nil"))
               end
         end
         print()
    end
end

function read_query_result(inj)
    print(orangetext("read_query_result"))
    printCS()
    print("ROWS: "..type(inj.resultset.rows))
    if inj.resultset.rows ~= nil then
        print_fields(inj.resultset.fields)
        print("finish fields")
        print_rows(inj.resultset.rows)
    end
end
