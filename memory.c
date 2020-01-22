#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include "memory.h"

#define PAGE_SIZE 4096

static void *alloc_from_ram(size_t size)
{
	assert((size % PAGE_SIZE) == 0 && "size must be multiples of 4096");
	void* base = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
	if (base == MAP_FAILED)
	{
		printf("Unable to allocate RAM space\n");
		exit(0);
	}
	return base;
}

static void free_ram(void *addr, size_t size)
{
	munmap(addr, size);
}

struct node* free_list_head[13]={};

struct node
{
	struct node* prev;
	struct node* next;
};
struct metadata
{
	int allocation_size;
	int free_bytes_available;
};
void myfree(void *ptr)
{
	struct metadata* metadata_address=(struct metadata*)((unsigned long)(ptr)& ~0xfff);
	if(metadata_address->allocation_size>=PAGE_SIZE && metadata_address->free_bytes_available==-1)
	{
		free_ram(metadata_address,metadata_address->allocation_size);
	}
	else
	{
		metadata_address->free_bytes_available+=metadata_address->allocation_size;
		int power=0;
		int bucket_size=metadata_address->allocation_size;
		// printf("%d bucket %d\n",bucket_size,metadata_address->free_bytes_available );
		while(bucket_size!=1)
		{
			power++;
			bucket_size/=2;
			// printf("%d sdde\n", bucket_size);
		}

		bucket_size=metadata_address->allocation_size;
		struct node* my_node=(struct node*)ptr;
		my_node -> next=NULL;
		my_node-> prev=NULL;
		// printf("%d df\n",power );
		if (free_list_head[power]==NULL)
		{
			free_list_head[power] = my_node;
			// printf("head\n");
		}
		else
		{
		
			my_node->next = (struct node*)free_list_head[power];
			// printf("%p\n",free_list_head[power]);
			free_list_head[power]->prev = my_node;
			// printf("f\n");
			free_list_head[power] = my_node;
		}
		// printf("%d  df %d\n",bucket_size,metadata_address->free_bytes_available );
		if(metadata_address->free_bytes_available == PAGE_SIZE)
		{
			// if(bucket_size<2000)
			// printf("%d  sd %d\n",metadata_address->free_bytes_available ,bucket_size );
			int num_del=(PAGE_SIZE-16)/bucket_size;
			if(bucket_size == PAGE_SIZE)
				num_del=1;
			// if(bucket_size<2000)
			// printf("%d   %d\n",power,num_del );

			struct node* temp=free_list_head[power];
			while(num_del)
			{
				// printf("%d fd %p \n",num_del ,temp);
				if((struct metadata*)((unsigned long)(temp)& ~0xfff)==metadata_address)
				{
					num_del--;
					// printf("%d yeah \n",num_del );
					if(temp->next==NULL)
					{
						if(temp->prev==NULL)
						{
							free_list_head[power]=NULL;
							temp=NULL;
						}
						else
						{
							temp->prev->next=NULL;
							temp=temp->next;
						}
					}
					else if(temp->prev==NULL)
					{
						free_list_head[power]=temp->next;
						temp=free_list_head[power];
						temp->prev=NULL;
					}
					else
					{
						temp->prev->next=temp->next;
						temp->next->prev = temp->prev;
						temp=temp->next;
					}

				}
				else
					temp=temp->next;


			}
			free_ram(metadata_address,PAGE_SIZE);
		}

	}
}

void *mymalloc(size_t size)
{
	int power=0,bucket_size,temp_size=size;
	if(size>4080)
		temp_size += 16;
	//nearest power of 2 greater than or equal to size
	while(temp_size != 0)
	{
		temp_size >>= 1;
		power++;
	}
	bucket_size = 1<<power;

	if(size>4080)
	{
		void* page =  alloc_from_ram(bucket_size);
		struct metadata* page_data = page;
		page_data->allocation_size = bucket_size;
		page_data->free_bytes_available = -1;
		return page + 16;
	}

	else
	{
		if( free_list_head[power] == NULL ) //no memory object of required size
		{
			void* page =  alloc_from_ram(PAGE_SIZE);
			struct metadata* page_data = page;
			void* upper_limit=page+PAGE_SIZE -bucket_size;
			if(bucket_size == PAGE_SIZE)
				upper_limit+=bucket_size;
			for( void* i = page + 16 ; i < upper_limit ; i += bucket_size)
			{
				struct node* my_node = (struct node*)i;
				my_node->prev = NULL;
				my_node->next = NULL;

				if (free_list_head[power]==NULL)
				{
					free_list_head[power] = my_node;
				}
				else
				{
					my_node->next = free_list_head[power];
					free_list_head[power]->prev = my_node;
					free_list_head[power] = my_node;
				}
			}
			
			page_data->allocation_size = bucket_size;
			page_data->free_bytes_available = 4096;
		}
		

		free_list_head[power]->prev=NULL;
		struct node* ptr=(struct node*)free_list_head[power];
		struct metadata* metadata_address=(struct metadata*)((unsigned long long)(ptr)& ~0xfff);
		// int count=0;
			// struct node* my_no=free_list_head[power];
			// while(my_no!=NULL)
			// {
			// 	count++;
			// 	my_no=my_no->next;
			// }
			// printf("%d count =  free = %d\n",count,metadata_address->free_bytes_available );
		metadata_address->free_bytes_available -= bucket_size;

	// 	if(bucket_size==512)
	// 	{
	// 	printf("%d %p \n", metadata_address->free_bytes_available,metadata_address);
	// }
		// ((struct node*)(ptr))->next=NULL;
		free_list_head[power]=free_list_head[power]->next;
		if(free_list_head[power]!=NULL)
			free_list_head[power]->prev=NULL;
		ptr->next=NULL;
		// printf("%p 2s \n",ptr );

		// count=0;
		// 	my_no=free_list_head[power];
		// 	while(my_no!=NULL)
		// 	{
		// 		count++;
		// 		my_no=my_no->next;
		// 	}
			// printf("%d count = %d  %d\n ",count, bucket_size,metadata_address->free_bytes_available );
		return ptr;

	}
	
}
