/**
*打开binder驱动
*/
static int binder_open(struct inode *nodp, struct file *filp)
{
    struct binder_proc *proc; // 表示进程

    proc = kzalloc(sizeof(*proc), GFP_KERNEL); // 为binder_proc分配内存空间
   
    //下面主要是对binder_proc里面的成员进行初始化
    INIT_LIST_HEAD(&proc->todo); //初始化todo列表
    init_waitqueue_head(&proc->wait); //初始化wait队列

    // ......

    filp->private_data = proc; //将binder_proc对象保存到filp->private_data中

    return 0;
}



/**
*为binder分配内存，并同时映射到内核空间和用户空间
*/
static int binder_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct vm_struct *area;//area表示内核虚拟空间，vma表示用户虚拟空间
	struct binder_proc *proc = filp->private_data;

    //分配内核虚拟空间
	area = get_vm_area(vma->vm_end - vma->vm_start, VM_IOREMAP);
	
	//记录binder内核缓冲区地址
	proc->buffer = area->addr;
	//记录用户空间与内核空间的地址差值
	proc->user_buffer_offset = vma->vm_start - (uintptr_t)proc->buffer;
	//记录binder内核缓冲区大小
	proc->buffer_size = vma->vm_end - vma->vm_start;

    //分配物理页面，并同时映射到内核空间和用户空间
	binder_update_page_range(proc, 1, proc->buffer, proc->buffer + PAGE_SIZE, vma);

	return 0;
}



/**
*用于操作binder驱动，比如数据读写
*/
static long binder_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct binder_proc *proc = filp->private_data;
	struct binder_thread *thread;
	void __user *ubuf = (void __user *)arg;

    //内核为每一个线程创建一个binder_thread结构体
    //先从缓存中查找，没有则创建
	thread = binder_get_thread(proc);

	switch (cmd) {
	case BINDER_WRITE_READ: {//数据读写命令
		//用于接收从用户传来的数据
		struct binder_write_read bwr;
		//将数据从用户空间拷贝到内核空间(只拷贝结构体，没有拷贝数据本身)
		copy_from_user(&bwr, ubuf, sizeof(bwr));
		
		if (bwr.write_size > 0) {
			//发送数据
			ret = binder_thread_write(proc, thread, (void __user *)bwr.write_buffer, bwr.write_size, &bwr.write_consumed);
		}
		if (bwr.read_size > 0) {
			//接收数据，没有数据时该方法阻塞
			ret = binder_thread_read(proc, thread, (void __user *)bwr.read_buffer, bwr.read_size, &bwr.read_consumed, filp->f_flags & O_NONBLOCK);
		}
		break;
	}
	case BINDER_SET_CONTEXT_MGR://成为ServiceManager的命令
	    //创建一个全局的binder_node节点，在内核中表示ServiceManager
		binder_context_mgr_node = binder_new_node(proc, NULL, NULL);
		break;
	}
	return 0;
}

/**
*用户空间和用户空间传送数据的数据结构
*/
struct binder_write_read {
	signed long	write_size;//发送缓冲区大小
	signed long	write_consumed;//offset
	unsigned long	write_buffer;//发送缓冲区

	signed long	read_size;//接收缓冲区
	signed long	read_consumed;//offset
	unsigned long	read_buffer;//接收缓冲区
};

/**
*处理应用进程向binder驱动发送数据
*/
int binder_thread_write(struct binder_proc *proc, struct binder_thread *thread,
			void __user *buffer, int size, signed long *consumed)
{
	uint32_t cmd;
	void __user *ptr = buffer + *consumed;

    get_user(cmd, (uint32_t __user *)ptr);//获取命令
		
	ptr += sizeof(uint32_t);//移动指针，指向命令后面的数据
	
	switch (cmd) {
	case BC_TRANSACTION://client发往binder驱动的命令
	case BC_REPLY: {//server发往驱动的命令
		struct binder_transaction_data tr;

        //将binder_transaction_data结构体从用户空间拷到内核空间
		copy_from_user(&tr, ptr, sizeof(tr));
		//处理数据传输
		binder_transaction(proc, thread, &tr, cmd == BC_REPLY);
		break;
	  }

    }

	return 0;
}



static void binder_transaction(struct binder_proc *proc,
			       struct binder_thread *thread,
			       struct binder_transaction_data *tr, int reply)
{
	struct binder_transaction *t;
	size_t *offp, *off_end;
	struct binder_proc *target_proc;
	struct binder_thread *target_thread = NULL;
	struct binder_node *target_node = NULL;
	struct list_head *target_list;//todo队列
	wait_queue_head_t *target_wait;//等待队列
	struct binder_transaction *in_reply_to = NULL;


	//下面是数据包的路由过程
	// handle -> binder_ref -> binder_node -> binder_proc
	

	if (reply) {//server进程发给驱动的数据
		in_reply_to = thread->transaction_stack;
		thread->transaction_stack = in_reply_to->to_parent;

		target_thread = in_reply_to->from;//找到需要回复的线程
		target_proc = target_thread->proc;
	} else {//client进程发给驱动的数据

		if (tr->target.handle) {// handle != 0，表示目标服务为普通Server

			struct binder_ref *ref;

			//通过handle找到binder_ref
			ref = binder_get_ref(proc, tr->target.handle);

			//通过binder_ref找到binder_node
			target_node = ref->node;

		} else {// handle == 0，表示目标服务为ServiceManager
			target_node = binder_context_mgr_node;
		}
		
		//通过binder_node找到目标进程
		target_proc = target_node->proc;
        //从目标进程的空闲线程中寻找一个合适的线程处理任务
		target_thread = . . . 
		
	}

	//下面这一段代码的目的是找到目标进程或目标线程的todo队列和wait队列

	if (target_thread) {//找到合适的线程就交由该线程处理
		target_list = &target_thread->todo;
		target_wait = &target_thread->wait;
	} else {//否则交给该线程所属进程处理
		target_list = &target_proc->todo;
		target_wait = &target_proc->wait;
	}
	
    //创建binder_transaction结构体
	t = kzalloc(sizeof(*t), GFP_KERNEL);


	if (!reply && !(tr->flags & TF_ONE_WAY))//同步，记录发送端线程，以便给它发送应答数据
		t->from = thread;
	else
		t->from = NULL;

	//下面这段代码的目的是将binder_transaction_data进一步封装成binder_transaction

	t->sender_euid = proc->tsk->cred->euid;
	t->to_proc = target_proc;
	t->to_thread = target_thread;
	t->code = tr->code;
	t->flags = tr->flags;
	t->priority = task_nice(current);
	//在接收端分配内核缓存区，用来接收发送端发送的数据
	t->buffer = binder_alloc_buf(target_proc, tr->data_size, tr->offsets_size, !reply && (t->flags & TF_ONE_WAY));
	t->buffer->allow_user_free = 0;
	t->buffer->debug_id = t->debug_id;
	t->buffer->transaction = t;
	t->buffer->target_node = target_node;
	
	offp = (size_t *)(t->buffer->data + ALIGN(tr->data_size, sizeof(void *)));
    //将发送端的数据拷贝到接收端的内核缓冲区（到这里才真正拷贝数据）
	copy_from_user(t->buffer->data, tr->data.ptr.buffer, tr->data_size);
	copy_from_user(offp, tr->data.ptr.offsets, tr->offsets_size);


	//下面这段循环代码对发送数据中的binder服务对象和binder代理对象进行特殊处理
	//如果只是普通数据，这段代码不会执行
		

	off_end = (void *)offp + tr->offsets_size;

	for (; offp < off_end; offp++) {
		//flat_binder_object是对binder对象的包装
		struct flat_binder_object *fp;
		fp = (struct flat_binder_object *)(t->buffer->data + *offp);

		switch (fp->type) {
		case BINDER_TYPE_BINDER:{//处理binder服务对象

			struct binder_ref *ref;

			struct binder_node *node = binder_get_node(proc, fp->binder);
			if (node == NULL) {
				//在发送端binder_proc的红黑树中创建binder_node
				node = binder_new_node(proc, fp->binder, fp->cookie);
			}
		
		    //在接收端binder_proc的红黑树中创建binder_ref
		    //这里内核会为服务分配唯一的handle值，保存在ref->desc中
			ref = binder_get_ref_for_node(target_proc, node);
			
			if (fp->type == BINDER_TYPE_BINDER)
				fp->type = BINDER_TYPE_HANDLE;
			else
				fp->type = BINDER_TYPE_WEAK_HANDLE;

			fp->handle = ref->desc;//desc即为服务的handle值
			
		} break;

		case BINDER_TYPE_HANDLE: {//处理binder代理对象

			struct binder_ref *ref = binder_get_ref(proc, fp->handle);
		
			if (ref->node->proc == target_proc) {//如果client和server运行在同一个进程
				if (fp->type == BINDER_TYPE_HANDLE)
					fp->type = BINDER_TYPE_BINDER;//更改binder类型
				else
					fp->type = BINDER_TYPE_WEAK_BINDER;

				fp->binder = ref->node->ptr;
				fp->cookie = ref->node->cookie;

			} else {//如果client和server运行在不同进程

				struct binder_ref *new_ref;
				//在接收端binder_proc的红黑树中创建binder_ref
				new_ref = binder_get_ref_for_node(target_proc, ref->node);
				fp->handle = new_ref->desc;
			}
		} break;
	    
	    }//switch end

    }//for end
	
	t->work.type = BINDER_WORK_TRANSACTION;

	//将binder_transaction加到接收端的todo列表，待处理
	list_add_tail(&t->work.entry, target_list);

	//唤醒接收端进程，处理binder_transaction
	wake_up_interruptible(target_wait);
}



/**
*处理应用进程从binder驱动读取数据
*/
static int binder_thread_read(struct binder_proc *proc,
			      struct binder_thread *thread,
			      void  __user *buffer, int size,
			      signed long *consumed, int non_block)
{
	void __user *ptr = buffer + *consumed;
	void __user *end = buffer + size;

	int ret = 0;
	int wait_for_proc_work;
    
    //判断当前线程是否空闲
	wait_for_proc_work = thread->transaction_stack == NULL && list_empty(&thread->todo);

	if (wait_for_proc_work) {
		//当前线程空闲，它在进程的等待队列上休眠，可以处理该进程的任务
		ret = wait_event_interruptible_exclusive(proc->wait, binder_has_proc_work(proc, thread));
	} else {
		//当前线程还有待处理的任务，它在自己的等待队列上休眠，只能处理自己的任务
		ret = wait_event_interruptible(thread->wait, binder_has_thread_work(thread));
	}
	
	//该线程被其他线程唤醒后，继续往下执行

	while (1) {
		uint32_t cmd;
		struct binder_transaction_data tr;
		struct binder_work *w;
		struct binder_transaction *t = NULL;

        //从进程或线程的todo队列把任务取出
		if (!list_empty(&thread->todo))
			w = list_first_entry(&thread->todo, struct binder_work, entry);
		else if (!list_empty(&proc->todo) && wait_for_proc_work)
			w = list_first_entry(&proc->todo, struct binder_work, entry);

        //binder_work转换成binder_transaction
		switch (w->type) {
		case BINDER_WORK_TRANSACTION: 
			t = container_of(w, struct binder_transaction, work);
		    break;
		}

	    //下面这段代码把binder_transaction格式的数据转换成binder_transaction_data格式

		if (t->buffer->target_node) {//驱动发往server
			struct binder_node *target_node = t->buffer->target_node;
			tr.target.ptr = target_node->ptr;
			tr.cookie =  target_node->cookie;

			cmd = BR_TRANSACTION;
		} else {//驱动发往client
			tr.target.ptr = NULL;
			tr.cookie = NULL;
			cmd = BR_REPLY;
		}
		tr.code = t->code;
		tr.flags = t->flags;
		tr.sender_euid = t->sender_euid;

		tr.data_size = t->buffer->data_size;
		tr.offsets_size = t->buffer->offsets_size;
		//把内核空间地址转换成用户空间地址，以便用户进程可以通过该地址取到数据
		tr.data.ptr.buffer = (void *)t->buffer->data + proc->user_buffer_offset;
		tr.data.ptr.offsets = tr.data.ptr.buffer + ALIGN(t->buffer->data_size, sizeof(void *));


        //数据拷贝到用户空间
		put_user(cmd, (uint32_t __user *)ptr);
		ptr += sizeof(uint32_t);
		copy_to_user(ptr, &tr, sizeof(tr));
		
		break;//退出循环
	}

	return 0;
}





/**
*内核中表示进程的结构体
*/
struct binder_proc {
	struct rb_root threads;//binder_thread红黑树的根节点，表示进程中的binder线程

	struct rb_root nodes;//binder_node红黑树的根节点

	struct rb_root refs_by_desc;//binder_ref红黑树的根节点(以handle为key)
	struct rb_root refs_by_node;//binder_ref红黑树的根节点（以binder_node为key）

	void *buffer;//内核缓存区地址
	ptrdiff_t user_buffer_offset;//内核空间与用户空间的地址偏移量
	size_t buffer_size;//内核缓存区大小
	
	struct list_head todo;//todo队列，即该进程需要处理的任务
	wait_queue_head_t wait;//等待队列，进程空闲时在此队列上休眠
};

/**
*内核中表示线程的结构体
*/
struct binder_thread {
	struct binder_proc *proc;//所属进程

	struct binder_transaction *transaction_stack;//正在处理的事务

	struct list_head todo;//todo队列，即该进程需要处理的任务
	wait_queue_head_t wait;//等待队列，进程空闲时在此队列上休眠
};