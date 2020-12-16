Workflow
========

.. uml::

   start
   :store.st file/
   :Generate store
   (by libstored's CMakeLists.txt);
   fork
      :C++ static library/
      :Link to common libstored library;
      :Integrate store in project;
      :Instantiate Debugger instance,
      and build protocol stack;
      :Instantiate Synchronizer instance,
      and build protocol stack;
   fork again
      :VHDL package and entity/
      :Include common libstored
      package/entities;
      :Integrate store in project;
      :Build store's synchronizer
      protocol stack;
   fork again
      :Documentation/
      :Integrate in project's docs;
   end fork
   :Integrate on device;
   :Debug with Python client;
   stop

