function sum_array()
    local arr = {}
    for i = 0, 9999 do arr[i] = i end
    local total = 0
    for iter = 0, 999 do
        for i = 0, 9999 do total = total + arr[i] end
    end
    return total
end
sum_array()
