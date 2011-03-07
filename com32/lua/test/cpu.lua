  cpuflags = cpu.flags()

  print("Vendor = " .. cpuflags["vendor"])
  print("Model = " .. cpuflags["model"])
  print("Cores = " .. cpuflags["cores"])
  print("L1 Instruction Cache = " .. cpuflags["l1_instruction_cache"])
  print("L1 Data Cache = " .. cpuflags["l1_data_cache"])
  print("L2 Cache = " .. cpuflags["l2_cache"])
  print("Family ID = " .. cpuflags["family_id"])
  print("Model ID = " .. cpuflags["model_id"])
  print("Stepping = " .. cpuflags["stepping"])

  if ( string.match(cpuflags["vendor"], "Intel") ) then
    print("Intel Processor Found")
--    syslinux.run_command("memdisk initrd=/dos/BIOS/FSC-P7935-108.img raw")
  else
    print("Does not match")
  end

