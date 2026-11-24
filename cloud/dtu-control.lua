function()
    local post="$sys/YOUR_PRODUCT_ID/demo_device/thing/property/post"
    local reply="$sys/YOUR_PRODUCT_ID/demo_device/thing/property/set_reply"
    local pending=""
    PronetStopProRecCh(1)
    UartStopProRecCh(1)
    PronetMqttProReciveTopic(1,1,",")
    local function publish(frame)
        frame=frame:match("^%s*(.-)%s*$")
        local topic,data=post,frame
        if frame:sub(1,1)~="{" then
            local comma=frame:find(",",1,true)
            if not comma then return false end
            topic,data=frame:sub(1,comma-1),frame:sub(comma+1)
        end
        if topic~=post and topic~=reply then return false end
        local ok,value=pcall(json.decode,data)
        if not ok or type(value)~="table" then return false end
        PronetSetSendCh(1,{topic,data})
        return true
    end
    while true do
        local incoming=PronetGetRecChAndDel(1)
        if incoming then UartSetSendCh(1,incoming.."\r\n") end
        local serial=UartGetRecChAndDel(1)
        if serial then
            pending=pending..serial
            while true do
                local cut=pending:find("\n",1,true)
                if not cut then break end
                local frame=pending:sub(1,cut-1)
                pending=pending:sub(cut+1)
                if #frame>0 then publish(frame) end
            end
            if #pending>0 and publish(pending) then pending="" end
            if #pending>4096 then pending="" end
        end
        sys.wait(20)
    end
end
