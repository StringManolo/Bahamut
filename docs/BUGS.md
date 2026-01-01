# List of found bugs

#### Unable to install modules unless shared folder is removed
If **shared** folder exists; new modules will not run npm install cuz folder exists. This was implemented to prevent modules from running **npm i** each time they are run. Since **installCommand** in `Install: installCommand`can be yarn, pnmp, or a git clone that copy and paste a module into a folder, there is not an easy way to track the node_moduels / ESM .mjs custom modules.  
  
Fix: Need to think a good way to fix this. Running the install command each time the module needs to be run should be better in most cases, but will slow down the module speed. Also if the install command for example downloads a 100GB model or something like that, this could make the software useless. So a better fix is needed.

#### Unable to specify multiple sources to Consume from modules
Modules should be able to specify more than 1 value. Currently you can only specify 1 or all *  
  
Fix: Check docu if any way is already described to implement, if not evaluate between using a special separator like: Consume: domain | proxyhttp | ...  
Or Consume domain\nConsume proxyhttp\n ...
Or both.

#### Delete is unable to delete if Consume and Provide are not the same
This is not intended.  
  
Fix: There should be a better way to Delete specified memory registers to liberate core.cpp memory as indicated by any module. DeleteAll might be needed too.


