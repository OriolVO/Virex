function nested_loops()
    local sum = 0
    for i = 0, 999 do
        for j = 0, 999 do sum = sum + 1 end
    end
    return sum
end
nested_loops()
