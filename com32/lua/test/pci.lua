-- get nice output
printf = function(s,...)
           return io.write(s:format(...))
         end

-- get device info
pciinfo = pci.getinfo()

-- get plain text device description
pciids = pci.getidlist("/pci.ids")

-- list all pci busses
for dind,device in pairs(pciinfo) do

  -- search for device description
  search = string.format("%04x%04x", device['vendor'], device['product'])

  printf(" %04x:%04x:%04x:%04x = ", device['vendor'], device['product'],
			device['sub_vendor'], device['sub_product'])

  if ( pciids[search] ) then
         printf("%s\n", pciids[search])
  else
         printf("Unknown\n")
  end
end

-- print(pciids["8086"])
-- print(pciids["10543009"])
-- print(pciids["00700003"])
-- print(pciids["0070e817"])
-- print(pciids["1002437a1002437a"])


