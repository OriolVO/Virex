function sieve_primes(limit)
    local is_prime = {}
    for i = 0, limit - 1 do is_prime[i] = true end
    is_prime[0] = false; is_prime[1] = false
    local i = 2
    while i * i < limit do
        if is_prime[i] then
            local j = i * i
            while j < limit do is_prime[j] = false; j = j + i end
        end
        i = i + 1
    end
    local count = 0
    for i = 0, limit - 1 do if is_prime[i] then count = count + 1 end end
    return count
end
local total = 0
for iter = 0, 999 do
    total = total + sieve_primes(10000)
end
