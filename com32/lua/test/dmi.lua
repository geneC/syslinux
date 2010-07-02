if (dmi.supported()) then

  dmitable = dmi.gettable()

  for k,v in pairs(dmitable) do
    print(k, v)
  end

  print(dmitable["system.manufacturer"])
  print(dmitable["system.product_name"])
  print(dmitable["bios.bios_revision"])

  if ( string.match(dmitable["system.product_name"], "ESPRIMO P7935") ) then
    print("Matches")
    syslinux.run_command("memdisk initrd=/dos/BIOS/FSC-P7935-108.img raw")
  else
    print("Does not match")
  end

end

