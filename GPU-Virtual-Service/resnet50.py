import torch
import torch.nn as nn
import torchvision
from torch.autograd import Variable
import matplotlib.pyplot as plt
import torch.nn.functional as F
import torch.utils.data as Data
import time

device = torch.device("cuda")

Epoch=100
Batch_Size=128
LR=0.01

#训练集
trainData=torchvision.datasets.MNIST(
    root="./data",
    train=True,
    transform=torchvision.transforms.ToTensor(),
    download=False)

train_loader=Data.DataLoader(dataset=trainData,batch_size=Batch_Size,shuffle=True)
test_data=torchvision.datasets.MNIST(root="./data",train=False,download=False)

test_x = torch.unsqueeze(test_data.data, dim=1).type(torch.FloatTensor)[:5000]/255. # shape from (2000, 28, 28) to (2000, 1, 28, 28), value in range(0,1)
test_y = test_data.targets[:5000]
test_result = test_y
test_x = test_x.to(device)
test_y = test_y.to(device)

#残差块
class ResidualBlock(nn.Module):
    def __init__(self,channel):
        super(ResidualBlock, self).__init__()
        self.channel=channel
        self.conv1=nn.Sequential(
            nn.Conv2d(in_channels=channel,
                      out_channels=channel,
                      kernel_size=3,
                      stride=1,
                      padding=1),
            nn.BatchNorm2d(channel),
            nn.ReLU(inplace=True)
        )
        self.conv2=nn.Sequential(
            nn.Conv2d(channel,channel,kernel_size=3,stride=1,padding=1),
            # nn.BatchNorm2d(channel)
        )
    def forward(self,x):
        out=self.conv1(x)
        out=self.conv2(out)
        out+=x
        out=F.relu(out)
        return out

#残差网络
class ResNet(nn.Module):
    def __init__(self):
        super(ResNet, self).__init__()
        self.conv1=nn.Sequential(
            nn.Conv2d(in_channels=1,out_channels=32,kernel_size=5), #(1,28,28)
            nn.BatchNorm2d(32),                                     #(32,24,24)
            nn.ReLU(),
            nn.MaxPool2d(2)                                         #(32,12,12)
        )
        self.conv2 = nn.Sequential(
            nn.Conv2d(in_channels=32, out_channels=16, kernel_size=5), #(16,8,8)
            nn.BatchNorm2d(16),
            nn.ReLU(),
            nn.MaxPool2d(2)                                           #(16,4,4)
        )
        self.reslayer1=ResidualBlock(32)
        self.reslayer2=ResidualBlock(16)
        self.fc=nn.Linear(256,10)              #这里的输入256是因为16*4*4=256

    def forward(self,x):
        out=self.conv1(x)
        out=self.reslayer1(out)
        out=self.conv2(out)
        out=self.reslayer2(out)
        out=out.view(out.size(0),-1)
        out=self.fc(out)
        return  out

#关于训练
def Train(Res):
    # 损失函数,以及优化器
    loss_func = nn.CrossEntropyLoss()
    loss_func = loss_func.to(device)
    optimizer = torch.optim.Adam(Res.parameters(), lr=LR)
    for epoch in range(Epoch):
        for step,(b_x,b_y)in enumerate(train_loader):
            b_x = b_x.to(device)
            b_y = b_y.to(device)
            output=Res(b_x)
            loss=loss_func(output,b_y)

            optimizer.zero_grad()
            loss.backward()
            optimizer.step()

            if(step%50==0):
                print('Epoch: ', epoch, '| train loss: %.4f' % loss.item())
    torch.save(Res, 'res_minist.pkl')
    print('res finish training')


x=torch.randn(16,1,28,28)
res=ResNet()
res=res.to(device)

# 测试
def Restest():
    res=torch.load('res_minist.pkl')
    res.to(device)
    test_output=res(test_x[:20])
    test_output = test_output.cpu()
    prediction=torch.max(test_output,1)[1].data.numpy()
    print(prediction, 'prediction number')
    print(test_result[:20].numpy(), 'real number')

    """
    test_output1 = res(test_x)
    pred_y1 = torch.max(test_output1, 1)[1].data.numpy()
    accuracy = float((pred_y1 == test_y.data.numpy()).astype(int).sum()) / float(test_y.size(0))
    print('accuracy', accuracy)
    """

if __name__=='__main__':
    Train(res)
    Restest()
